# Monta a pasta dist/ com o executavel e tudo que ele precisa para rodar em um
# computador que nao tem o Qt instalado.
#
# Copiar so o .exe nao basta, e o erro nao aparece na hora: falta uma DLL e o
# programa abre normalmente ate tocar a parte que depende dela. Foi assim que o
# driver de SQLite ficou de fora e o banco de retratos falhava em silencio.
#
# Uso:
#   $env:QT_ROOT = "D:/Qt/6.8.3/mingw_64"
#   .\scripts\empacotar.ps1

$ErrorActionPreference = "Stop"

$raiz = Split-Path -Parent $PSScriptRoot
$qt = $env:QT_ROOT
if (-not $qt) {
    throw "Defina QT_ROOT apontando para a instalacao do Qt (ex.: D:/Qt/6.8.3/mingw_64)"
}

$qt = $qt -replace "/", "\"
$exe = Join-Path $raiz "build\release\zelo.exe"
$dist = Join-Path $raiz "dist"

# O windeployqt precisa das proprias DLLs do Qt no PATH. Sem isso ele encerra
# com codigo 5 e nao imprime nada — chamar pelo caminho absoluto nao basta, e o
# silencio faz parecer que deu certo.
#
# O MinGW da instalacao do Qt vem junto, e antes de qualquer outro: uma copia
# antiga de MinGW no PATH do sistema entrega um libstdc++ incompativel e o
# windeployqt morre com violacao de acesso, sem mensagem alguma.
$mingw = Get-ChildItem (Join-Path $qt "..\..\Tools") -Directory -Filter "mingw*" `
    -ErrorAction SilentlyContinue | Select-Object -Last 1

if ($mingw) {
    $env:PATH = "$($mingw.FullName)\bin;$env:PATH"
}
$env:PATH = "$qt\bin;$env:PATH"

if (-not (Test-Path $exe)) {
    throw "Compile primeiro: cmake --build --preset release"
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item $exe $dist -Force

# O windeployqt resolve as dependencias do Qt lendo o proprio executavel, entao
# uma DLL nova nunca fica esquecida por descuido de quem manteria uma lista.
#
# Ele escreve avisos inofensivos na saida de erro. Com a preferencia
# em "Stop", um aviso derruba o script inteiro — o que importa aqui e o codigo
# de saida, conferido logo abaixo.
$anterior = $ErrorActionPreference
$ErrorActionPreference = "Continue"

& (Join-Path $qt "bin\windeployqt.exe") `
    --release `
    --no-translations `
    --no-opengl-sw `
    (Join-Path $dist "zelo.exe")

$codigo = $LASTEXITCODE
$ErrorActionPreference = $anterior

if ($codigo -ne 0) {
    throw "windeployqt falhou com codigo $codigo"
}

# As do compilador nao passam pelo windeployqt.
if ($mingw) {
    foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
        $origem = Join-Path $mingw.FullName "bin\$dll"
        if (Test-Path $origem) {
            Copy-Item $origem $dist -Force
        }
    }
}

# Conferencia final. O driver de SQLite mora em uma subpasta e nao aparece em
# uma listagem rasa: sem ele, o banco de retratos nao abre.
$sql = Join-Path $dist "Qt6Sql.dll"
$driver = Join-Path $dist "sqldrivers\qsqlite.dll"

if (-not (Test-Path $sql) -or -not (Test-Path $driver)) {
    throw "Faltou o suporte a SQLite em dist/. O historico e os retratos nao funcionariam."
}

Write-Host "dist/ pronto:" (Get-ChildItem $dist -Recurse -File).Count "arquivos"
