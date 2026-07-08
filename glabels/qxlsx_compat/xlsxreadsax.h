// stub: private header not shipped by MSYS2 qxlsx.
// Only the types referenced by xlsxdocument.h's read_sheet_sax signatures are
// provided (we never call those methods); the real implementation lives in the DLL.
#ifndef XLSXREADSAX_H_STUB
#define XLSXREADSAX_H_STUB

#include <functional>
#include <QString>

QT_BEGIN_NAMESPACE_XLSX

struct sax_options {};
using sax_cell_callback = std::function<void(int, int, const QString&)>;

QT_END_NAMESPACE_XLSX

#endif
