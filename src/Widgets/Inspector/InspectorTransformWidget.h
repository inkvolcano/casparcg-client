#pragma once

#include "../Shared.h"

#include "Commands/TransformData.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorTransformWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit InspectorTransformWidget(QWidget* parent = nullptr);

    private:
        TransformData* transformData = nullptr;
        bool blockUpdates = false;

        // Fill
        QCheckBox* checkFill = nullptr;
        QDoubleSpinBox* spinFillX = nullptr;
        QDoubleSpinBox* spinFillY = nullptr;
        QDoubleSpinBox* spinFillScaleX = nullptr;
        QDoubleSpinBox* spinFillScaleY = nullptr;

        // Opacity
        QCheckBox* checkOpacity = nullptr;
        QDoubleSpinBox* spinOpacity = nullptr;

        // Rotation
        QCheckBox* checkRotation = nullptr;
        QDoubleSpinBox* spinRotation = nullptr;

        // Crop
        QCheckBox* checkCrop = nullptr;
        QDoubleSpinBox* spinCropLeft = nullptr;
        QDoubleSpinBox* spinCropTop = nullptr;
        QDoubleSpinBox* spinCropRight = nullptr;
        QDoubleSpinBox* spinCropBottom = nullptr;

        // Anchor
        QCheckBox* checkAnchor = nullptr;
        QDoubleSpinBox* spinAnchorX = nullptr;
        QDoubleSpinBox* spinAnchorY = nullptr;

        // Clip
        QCheckBox* checkClip = nullptr;
        QDoubleSpinBox* spinClipX = nullptr;
        QDoubleSpinBox* spinClipY = nullptr;
        QDoubleSpinBox* spinClipW = nullptr;
        QDoubleSpinBox* spinClipH = nullptr;

        // Brightness / Contrast / Saturation / Volume
        QCheckBox* checkBrightness = nullptr;
        QDoubleSpinBox* spinBrightness = nullptr;
        QCheckBox* checkContrast = nullptr;
        QDoubleSpinBox* spinContrast = nullptr;
        QCheckBox* checkSaturation = nullptr;
        QDoubleSpinBox* spinSaturation = nullptr;
        QCheckBox* checkVolume = nullptr;
        QDoubleSpinBox* spinVolume = nullptr;

        // Entrance
        QCheckBox* checkEntrance = nullptr;
        QComboBox* comboEntranceProp = nullptr;
        QDoubleSpinBox* spinEntranceFrom = nullptr;
        QDoubleSpinBox* spinEntranceTo = nullptr;
        QSpinBox* spinEntranceDuration = nullptr;
        QComboBox* comboEntranceTween = nullptr;

        QDoubleSpinBox* createNormSpin(double value = 0.0, double min = -2.0, double max = 2.0);
        QWidget* createSliderSpin(QDoubleSpinBox*& spinOut, double value, double min, double max);
        void loadFromData();
        void syncToData();

        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
};
