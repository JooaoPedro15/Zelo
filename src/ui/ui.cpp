#include "ui/ui.hpp"

#include <QString>

namespace cleaner::ui {

std::string module_name() {
    return QStringLiteral("ui").toStdString();
}

}
