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
        QCheckBox* checkFill;
        QDoubleSpinBox* spinFillX;
        QDoubleSpinBox* spinFillY;
        QDoubleSpinBox* spinFillScaleX;
        QDoubleSpinBox* spinFillScaleY;

        // Opacity
        QCheckBox* checkOpacity;
        QDoubleSpinBox* spinOpacity;

        // Rotation
        QCheckBox* checkRotation;
        QDoubleSpinBox* spinRotation;

        // Crop
        QCheckBox* checkCrop;
        QDoubleSpinBox* spinCropLeft;
        QDoubleSpinBox* spinCropTop;
        QDoubleSpinBox* spinCropRight;
        QDoubleSpinBox* spinCropBottom;

        // Anchor
        QCheckBox* checkAnchor;
        QDoubleSpinBox* spinAnchorX;
        QDoubleSpinBox* spinAnchorY;

        // Clip
        QCheckBox* checkClip;
        QDoubleSpinBox* spinClipX;
        QDoubleSpinBox* spinClipY;
        QDoubleSpinBox* spinClipW;
        QDoubleSpinBox* spinClipH;

        // Brightness / Contrast / Saturation / Volume
        QCheckBox* checkBrightness;
        QDoubleSpinBox* spinBrightness;
        QCheckBox* checkContrast;
        QDoubleSpinBox* spinContrast;
        QCheckBox* checkSaturation;
        QDoubleSpinBox* spinSaturation;
        QCheckBox* checkVolume;
        QDoubleSpinBox* spinVolume;

        // Entrance
        QCheckBox* checkEntrance;
        QComboBox* comboEntranceProp;
        QDoubleSpinBox* spinEntranceFrom;
        QDoubleSpinBox* spinEntranceTo;
        QSpinBox* spinEntranceDuration;
        QComboBox* comboEntranceTween;

        QDoubleSpinBox* createNormSpin(double value = 0.0, double min = -2.0, double max = 2.0);
        QWidget* createSliderSpin(QDoubleSpinBox*& spinOut, double value, double min, double max);
        void loadFromData();
        void syncToData();

        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
};
