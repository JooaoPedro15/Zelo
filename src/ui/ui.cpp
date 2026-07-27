#include "ui/ui.hpp"

#include <QString>

namespace zelo::ui {

std::string module_name() {
    return QStringLiteral("ui").toStdString();
}

}
