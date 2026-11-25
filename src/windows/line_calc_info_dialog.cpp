#include "line_calc_info_dialog.h"

#include <string>

#include "res/linberakning_excel.h"

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn). Texten kommer ur
// res/linberakning_excel.md, som CMake bygger in som kLineCalcExcelMd.
void LineCalcInfoDialog::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    docView.setText(std::string(kalkyl::res::kLineCalcExcelMd));
}
