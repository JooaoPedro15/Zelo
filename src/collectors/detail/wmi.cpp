#include "collectors/detail/wmi.hpp"

#include "collectors/detail/text.hpp"

#include <spdlog/spdlog.h>

// windows.h antes dos demais: oleauto.h e wbemidl.h dependem dos tipos base.
#include <windows.h>

#include <oleauto.h>
#include <wbemidl.h>

namespace cleaner::collectors::detail {

namespace {

/// COM precisa ser inicializado por thread, e o Qt ja inicializa a thread
/// principal do aplicativo como STA. Nesse caso o Windows responde
/// RPC_E_CHANGED_MODE, que significa "ja inicializado de outro jeito" — a
/// thread esta pronta para usar COM, so nao no modelo pedido.
///
/// Tratar isso como falha derrubava as duas coletas WMI dentro do aplicativo,
/// enquanto nos testes, sem COM previo, tudo passava.
class ComScope {
public:
    ComScope() {
        const HRESULT result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        initialized_ = SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;

        // So desfaz o que esta chamada criou. Encerrar COM de quem inicializou
        // antes deixaria o resto do aplicativo sem COM.
        owns_ = result == S_OK || result == S_FALSE;
    }

    ~ComScope() {
        if (owns_) {
            ::CoUninitialize();
        }
    }

    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;
    ComScope(ComScope&&) = delete;
    ComScope& operator=(ComScope&&) = delete;

    [[nodiscard]] bool ok() const { return initialized_; }

private:
    bool initialized_ = false;
    bool owns_ = false;
};

/// `_bstr_t` do comutil.h depende de uma biblioteca que o MinGW nao fornece,
/// entao a BSTR e gerenciada aqui mesmo.
class Bstr {
public:
    explicit Bstr(const wchar_t* text) : value_(::SysAllocString(text)) {}

    ~Bstr() {
        if (value_ != nullptr) {
            ::SysFreeString(value_);
        }
    }

    Bstr(const Bstr&) = delete;
    Bstr& operator=(const Bstr&) = delete;
    Bstr(Bstr&&) = delete;
    Bstr& operator=(Bstr&&) = delete;

    [[nodiscard]] BSTR get() const { return value_; }

private:
    BSTR value_;
};

class WbemRow final : public WmiRow {
public:
    explicit WbemRow(IWbemClassObject* object) : object_(object) {}

    [[nodiscard]] std::optional<std::string> text(const wchar_t* property) const override {
        VARIANT value{};
        if (FAILED(object_->Get(property, 0, &value, nullptr, nullptr))) {
            return std::nullopt;
        }

        std::optional<std::string> result;
        if (value.vt == VT_BSTR && value.bstrVal != nullptr) {
            result = to_utf8(std::wstring_view(value.bstrVal, ::SysStringLen(value.bstrVal)));
        }
        ::VariantClear(&value);
        return result;
    }

    [[nodiscard]] std::optional<std::int64_t> number(const wchar_t* property) const override {
        VARIANT value{};
        if (FAILED(object_->Get(property, 0, &value, nullptr, nullptr))) {
            return std::nullopt;
        }

        std::optional<std::int64_t> result;
        switch (value.vt) {
        case VT_I4:
            result = value.lVal;
            break;
        case VT_UI4:
            result = static_cast<std::int64_t>(value.ulVal);
            break;
        case VT_I2:
            result = value.iVal;
            break;
        case VT_UI1:
            result = value.bVal;
            break;
        case VT_BSTR:
            // Contadores de 64 bits chegam como texto: o WMI nao tem um tipo
            // proprio para eles.
            if (value.bstrVal != nullptr) {
                try {
                    result = std::stoll(to_utf8(std::wstring_view(
                        value.bstrVal, ::SysStringLen(value.bstrVal))));
                } catch (const std::exception&) {
                    result = std::nullopt;
                }
            }
            break;
        default:
            break;
        }
        ::VariantClear(&value);
        return result;
    }

    [[nodiscard]] std::optional<bool> boolean(const wchar_t* property) const override {
        VARIANT value{};
        if (FAILED(object_->Get(property, 0, &value, nullptr, nullptr))) {
            return std::nullopt;
        }

        std::optional<bool> result;
        if (value.vt == VT_BOOL) {
            result = value.boolVal != VARIANT_FALSE;
        }
        ::VariantClear(&value);
        return result;
    }

private:
    IWbemClassObject* object_;
};

}

bool query_wmi(const wchar_t* wmi_namespace, const wchar_t* query,
               const std::function<void(const WmiRow&)>& handle) {
    const ComScope com;
    if (!com.ok()) {
        spdlog::warn("nao foi possivel inicializar COM para consultar o WMI");
        return false;
    }

    IWbemLocator* locator = nullptr;
    if (FAILED(::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWbemLocator, reinterpret_cast<void**>(&locator)))) {
        spdlog::warn("servico WMI indisponivel");
        return false;
    }

    IWbemServices* services = nullptr;
    const Bstr path(wmi_namespace);
    if (FAILED(locator->ConnectServer(path.get(), nullptr, nullptr, nullptr, 0, nullptr, nullptr,
                                      &services))) {
        spdlog::warn("nao foi possivel abrir o namespace WMI solicitado");
        locator->Release();
        return false;
    }

    // Sem isto a consulta roda com a identidade errada e falha por permissao.
    ::CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    IEnumWbemClassObject* enumerator = nullptr;
    const Bstr language(L"WQL");
    const Bstr text(query);
    if (FAILED(services->ExecQuery(language.get(), text.get(),
                                   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
                                   &enumerator))) {
        spdlog::warn("consulta WMI recusada");
        services->Release();
        locator->Release();
        return false;
    }

    constexpr ULONG kTimeoutMs = 10000;
    while (enumerator != nullptr) {
        IWbemClassObject* object = nullptr;
        ULONG returned = 0;

        // Limite de tempo por lote: o WMI trava em algumas maquinas, e a
        // analise nao pode ficar pendurada esperando.
        if (enumerator->Next(kTimeoutMs, 1, &object, &returned) != WBEM_S_NO_ERROR ||
            returned == 0) {
            break;
        }

        handle(WbemRow{object});
        object->Release();
    }

    if (enumerator != nullptr) {
        enumerator->Release();
    }
    services->Release();
    locator->Release();
    return true;
}

}
