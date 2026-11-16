#pragma once

#include "Shared.h"

#include <QtCore/QString>

class RundownTreeWidget;

// File > Export Run Sheet (build 304): the open rundown as a PDF for whoever
// is not at the client - a floor manager, a presenter, a producer.
//
// One line per item, groups indented: its name, type, channel and layer, its
// length and start time (the timing bar's own, back-timed to the hard out when
// the rundown has one - Common/RunSheetRules.h), its operator note, and its
// reference picture small beside it. Landscape A4.
namespace RunSheet
{
    WIDGETS_EXPORT bool exportPdf(RundownTreeWidget* rundown, const QString& path, QString* error);
}
