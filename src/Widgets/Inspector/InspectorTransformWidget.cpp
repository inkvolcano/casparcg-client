#include "InspectorTransformWidget.h"

#include "EventManager.h"

#include "Commands/TemplateCommand.h"
#include "Commands/MovieCommand.h"
#include "Commands/StillCommand.h"
#include "Commands/AudioCommand.h"
#include "Commands/HtmlCommand.h"
#include "Commands/ImageScrollerCommand.h"

InspectorTransformWidget::InspectorTransformWidget(QWidget* parent)
    : QWidget(parent)
{
    QGridLayout* grid = new QGridLayout(this);
    grid->setContentsMargins(4, 2, 4, 2);
    grid->setSpacing(2);

    int row = 0;

    // ── Fill ──
    checkFill = new QCheckBox("Fill", this);
    grid->addWidget(checkFill, row, 0);
    grid->addWidget(new QLabel("X", this), row, 1);
    grid->addWidget(createSliderSpin(spinFillX, 0.0, -2.0, 2.0), row, 2);
    grid->addWidget(new QLabel("Y", this), row, 3);
    grid->addWidget(createSliderSpin(spinFillY, 0.0, -2.0, 2.0), row, 4);
    row++;
    grid->addWidget(new QLabel("W", this), row, 1);
    grid->addWidget(createSliderSpin(spinFillScaleX, 1.0, 0.0, 20.0), row, 2);
    grid->addWidget(new QLabel("H", this), row, 3);
    grid->addWidget(createSliderSpin(spinFillScaleY, 1.0, 0.0, 20.0), row, 4);
    row++;

    // ── Opacity ──
    checkOpacity = new QCheckBox("Opacity", this);
    grid->addWidget(checkOpacity, row, 0);
    grid->addWidget(createSliderSpin(spinOpacity, 1.0, 0.0, 1.0), row, 2, 1, 3);
    row++;

    // ── Rotation ──
    checkRotation = new QCheckBox("Rotation", this);
    QWidget* rotWidget = createSliderSpin(spinRotation, 0.0, -720.0, 720.0);
    spinRotation->setSuffix(QString::fromUtf8("\xc2\xb0")); // °
    grid->addWidget(checkRotation, row, 0);
    grid->addWidget(rotWidget, row, 2, 1, 3);
    row++;

    // ── Anchor ──
    checkAnchor = new QCheckBox("Anchor", this);
    grid->addWidget(checkAnchor, row, 0);
    grid->addWidget(new QLabel("X", this), row, 1);
    grid->addWidget(createSliderSpin(spinAnchorX, 0.5, 0.0, 1.0), row, 2);
    grid->addWidget(new QLabel("Y", this), row, 3);
    grid->addWidget(createSliderSpin(spinAnchorY, 0.5, 0.0, 1.0), row, 4);
    row++;

    // ── Crop ──
    checkCrop = new QCheckBox("Crop", this);
    grid->addWidget(checkCrop, row, 0);
    grid->addWidget(new QLabel("L", this), row, 1);
    grid->addWidget(createSliderSpin(spinCropLeft, 0.0, 0.0, 1.0), row, 2);
    grid->addWidget(new QLabel("T", this), row, 3);
    grid->addWidget(createSliderSpin(spinCropTop, 0.0, 0.0, 1.0), row, 4);
    row++;
    grid->addWidget(new QLabel("R", this), row, 1);
    grid->addWidget(createSliderSpin(spinCropRight, 1.0, 0.0, 1.0), row, 2);
    grid->addWidget(new QLabel("B", this), row, 3);
    grid->addWidget(createSliderSpin(spinCropBottom, 1.0, 0.0, 1.0), row, 4);
    row++;

    // ── Clip ──
    checkClip = new QCheckBox("Clip", this);
    grid->addWidget(checkClip, row, 0);
    grid->addWidget(new QLabel("X", this), row, 1);
    grid->addWidget(createSliderSpin(spinClipX, 0.0, -2.0, 2.0), row, 2);
    grid->addWidget(new QLabel("Y", this), row, 3);
    grid->addWidget(createSliderSpin(spinClipY, 0.0, -2.0, 2.0), row, 4);
    row++;
    grid->addWidget(new QLabel("W", this), row, 1);
    grid->addWidget(createSliderSpin(spinClipW, 1.0, 0.0, 2.0), row, 2);
    grid->addWidget(new QLabel("H", this), row, 3);
    grid->addWidget(createSliderSpin(spinClipH, 1.0, 0.0, 2.0), row, 4);
    row++;

    // ── Brightness / Contrast / Saturation / Volume ──
    checkBrightness = new QCheckBox("Bright", this);
    grid->addWidget(checkBrightness, row, 0);
    grid->addWidget(createSliderSpin(spinBrightness, 1.0, 0.0, 4.0), row, 2, 1, 3);
    row++;

    checkContrast = new QCheckBox("Contrast", this);
    grid->addWidget(checkContrast, row, 0);
    grid->addWidget(createSliderSpin(spinContrast, 1.0, 0.0, 4.0), row, 2, 1, 3);
    row++;

    checkSaturation = new QCheckBox("Saturat.", this);
    grid->addWidget(checkSaturation, row, 0);
    grid->addWidget(createSliderSpin(spinSaturation, 1.0, 0.0, 4.0), row, 2, 1, 3);
    row++;

    checkVolume = new QCheckBox("Volume", this);
    grid->addWidget(checkVolume, row, 0);
    grid->addWidget(createSliderSpin(spinVolume, 1.0, 0.0, 4.0), row, 2, 1, 3);
    row++;

    // ── Entrance Animation ──
    checkEntrance = new QCheckBox("Entrance", this);
    comboEntranceProp = new QComboBox(this);
    comboEntranceProp->addItems({"opacity", "rotation", "brightness", "contrast", "saturation", "volume"});
    spinEntranceFrom = createNormSpin(0.0, -360.0, 360.0);
    spinEntranceTo = createNormSpin(1.0, -360.0, 360.0);
    spinEntranceDuration = new QSpinBox(this);
    spinEntranceDuration->setRange(0, 9999);
    spinEntranceDuration->setSuffix(" fr");
    comboEntranceTween = new QComboBox(this);
    comboEntranceTween->addItems({
        "Linear", "EaseNone", "EaseInQuad", "EaseOutQuad", "EaseInOutQuad",
        "EaseOutInQuad", "EaseInCubic", "EaseOutCubic", "EaseInOutCubic",
        "EaseOutInCubic", "EaseInQuart", "EaseOutQuart", "EaseInOutQuart",
        "EaseOutInQuart", "EaseInQuint", "EaseOutQuint", "EaseInOutQuint",
        "EaseOutInQuint", "EaseInSine", "EaseOutSine", "EaseInOutSine",
        "EaseOutInSine", "EaseInExponential", "EaseOutExponential",
        "EaseInOutExponential", "EaseOutInExponential", "EaseInCirc",
        "EaseOutCirc", "EaseInOutCirc", "EaseOutInCirc", "EaseInElastic",
        "EaseOutElastic", "EaseInOutElastic", "EaseOutInElastic",
        "EaseInBack", "EaseOutBack", "EaseInOutBack", "EaseOutInBack",
        "EaseInBounce", "EaseOutBounce", "EaseInOutBounce", "EaseOutInBounce"
    });

    grid->addWidget(checkEntrance, row, 0);
    grid->addWidget(comboEntranceProp, row, 2, 1, 3);
    row++;
    grid->addWidget(new QLabel("From", this), row, 1);
    grid->addWidget(spinEntranceFrom, row, 2);
    grid->addWidget(new QLabel("To", this), row, 3);
    grid->addWidget(spinEntranceTo, row, 4);
    row++;
    grid->addWidget(new QLabel("Dur", this), row, 1);
    grid->addWidget(spinEntranceDuration, row, 2);
    grid->addWidget(comboEntranceTween, row, 3, 1, 2);
    row++;

    // Connect all checkboxes and spinboxes to syncToData.
    auto connectCheck = [this](QCheckBox* cb) {
        QObject::connect(cb, &QCheckBox::toggled, this, &InspectorTransformWidget::syncToData);
    };
    auto connectSpin = [this](QDoubleSpinBox* sb) {
        QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &InspectorTransformWidget::syncToData);
    };
    auto connectSpinInt = [this](QSpinBox* sb) {
        QObject::connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, &InspectorTransformWidget::syncToData);
    };
    auto connectCombo = [this](QComboBox* cb) {
        QObject::connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InspectorTransformWidget::syncToData);
    };

    connectCheck(checkFill); connectCheck(checkOpacity); connectCheck(checkRotation);
    connectCheck(checkAnchor); connectCheck(checkCrop); connectCheck(checkClip);

    // Auto-enable anchor when rotation is checked (rotation needs a pivot point).
    QObject::connect(checkRotation, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && !checkAnchor->isChecked())
            checkAnchor->setChecked(true);
    });
    connectCheck(checkBrightness); connectCheck(checkContrast);
    connectCheck(checkSaturation); connectCheck(checkVolume);
    connectCheck(checkEntrance);

    connectSpin(spinFillX); connectSpin(spinFillY); connectSpin(spinFillScaleX); connectSpin(spinFillScaleY);
    connectSpin(spinOpacity); connectSpin(spinRotation);
    connectSpin(spinAnchorX); connectSpin(spinAnchorY);
    connectSpin(spinCropLeft); connectSpin(spinCropTop); connectSpin(spinCropRight); connectSpin(spinCropBottom);
    connectSpin(spinClipX); connectSpin(spinClipY); connectSpin(spinClipW); connectSpin(spinClipH);
    connectSpin(spinBrightness); connectSpin(spinContrast); connectSpin(spinSaturation); connectSpin(spinVolume);
    connectSpin(spinEntranceFrom); connectSpin(spinEntranceTo);
    connectSpinInt(spinEntranceDuration);
    connectCombo(comboEntranceProp); connectCombo(comboEntranceTween);

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)),
                     this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
}

QDoubleSpinBox* InspectorTransformWidget::createNormSpin(double value, double min, double max)
{
    QDoubleSpinBox* spin = new QDoubleSpinBox(this);
    spin->setRange(min, max);
    spin->setDecimals(3);
    spin->setSingleStep(0.01);
    spin->setValue(value);
    spin->setFixedHeight(22);
    return spin;
}

QWidget* InspectorTransformWidget::createSliderSpin(QDoubleSpinBox*& spinOut, double value, double min, double max)
{
    const int SLIDER_STEPS = 1000;

    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QSlider* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(0, SLIDER_STEPS);
    slider->setFixedHeight(18);
    slider->setValue(static_cast<int>((value - min) / (max - min) * SLIDER_STEPS));

    QDoubleSpinBox* spin = createNormSpin(value, min, max);
    spin->setFixedWidth(58);

    layout->addWidget(slider, 1);
    layout->addWidget(spin);

    // Slider → Spinbox sync.
    QObject::connect(slider, &QSlider::valueChanged, [spin, slider, min, max, SLIDER_STEPS]() {
        double val = min + (max - min) * slider->value() / SLIDER_STEPS;
        spin->blockSignals(true);
        spin->setValue(val);
        spin->blockSignals(false);
        // Emit manually so syncToData fires exactly once.
        emit spin->valueChanged(val);
    });

    // Spinbox → Slider sync.
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     [slider, min, max, SLIDER_STEPS](double val) {
        slider->blockSignals(true);
        slider->setValue(static_cast<int>((val - min) / (max - min) * SLIDER_STEPS));
        slider->blockSignals(false);
    });

    spinOut = spin;
    return container;
}

void InspectorTransformWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->transformData = nullptr;

    AbstractCommand* cmd = event.getCommand();
    if (cmd == nullptr)
        return;

    // Only content items have TransformData.
    if (auto* tc = dynamic_cast<TemplateCommand*>(cmd))
        this->transformData = &tc->getTransform();
    else if (auto* mc = dynamic_cast<MovieCommand*>(cmd))
        this->transformData = &mc->getTransform();
    else if (auto* sc = dynamic_cast<StillCommand*>(cmd))
        this->transformData = &sc->getTransform();
    else if (auto* ac = dynamic_cast<AudioCommand*>(cmd))
        this->transformData = &ac->getTransform();
    else if (auto* hc = dynamic_cast<HtmlCommand*>(cmd))
        this->transformData = &hc->getTransform();
    else if (auto* isc = dynamic_cast<ImageScrollerCommand*>(cmd))
        this->transformData = &isc->getTransform();

    loadFromData();
}

void InspectorTransformWidget::loadFromData()
{
    this->blockUpdates = true;

    if (this->transformData == nullptr)
    {
        // Clear all checkboxes.
        checkFill->setChecked(false); checkOpacity->setChecked(false);
        checkRotation->setChecked(false); checkAnchor->setChecked(false);
        checkCrop->setChecked(false); checkClip->setChecked(false);
        checkBrightness->setChecked(false); checkContrast->setChecked(false);
        checkSaturation->setChecked(false); checkVolume->setChecked(false);
        checkEntrance->setChecked(false);
        this->blockUpdates = false;
        return;
    }

    const TransformData& td = *this->transformData;

    checkFill->setChecked(td.fill.has_value());
    if (td.fill.has_value())
    {
        spinFillX->setValue(td.fill->x);
        spinFillY->setValue(td.fill->y);
        spinFillScaleX->setValue(td.fill->scaleX);
        spinFillScaleY->setValue(td.fill->scaleY);
    }

    checkOpacity->setChecked(td.opacity.has_value());
    if (td.opacity.has_value())
        spinOpacity->setValue(*td.opacity);

    checkRotation->setChecked(td.rotation.has_value());
    if (td.rotation.has_value())
        spinRotation->setValue(*td.rotation);

    checkAnchor->setChecked(td.anchor.has_value());
    if (td.anchor.has_value())
    {
        spinAnchorX->setValue(td.anchor->x);
        spinAnchorY->setValue(td.anchor->y);
    }

    checkCrop->setChecked(td.crop.has_value());
    if (td.crop.has_value())
    {
        spinCropLeft->setValue(td.crop->left);
        spinCropTop->setValue(td.crop->top);
        spinCropRight->setValue(td.crop->right);
        spinCropBottom->setValue(td.crop->bottom);
    }

    checkClip->setChecked(td.clip.has_value());
    if (td.clip.has_value())
    {
        spinClipX->setValue(td.clip->x);
        spinClipY->setValue(td.clip->y);
        spinClipW->setValue(td.clip->width);
        spinClipH->setValue(td.clip->height);
    }

    checkBrightness->setChecked(td.brightness.has_value());
    if (td.brightness.has_value()) spinBrightness->setValue(*td.brightness);

    checkContrast->setChecked(td.contrast.has_value());
    if (td.contrast.has_value()) spinContrast->setValue(*td.contrast);

    checkSaturation->setChecked(td.saturation.has_value());
    if (td.saturation.has_value()) spinSaturation->setValue(*td.saturation);

    checkVolume->setChecked(td.volume.has_value());
    if (td.volume.has_value()) spinVolume->setValue(*td.volume);

    checkEntrance->setChecked(td.entrance.has_value());
    if (td.entrance.has_value())
    {
        int idx = comboEntranceProp->findText(td.entrance->property);
        if (idx >= 0) comboEntranceProp->setCurrentIndex(idx);
        spinEntranceFrom->setValue(td.entrance->from);
        spinEntranceTo->setValue(td.entrance->to);
        spinEntranceDuration->setValue(td.entrance->duration);
        int tweenIdx = comboEntranceTween->findText(td.entrance->tween);
        if (tweenIdx >= 0) comboEntranceTween->setCurrentIndex(tweenIdx);
    }

    this->blockUpdates = false;
}

void InspectorTransformWidget::syncToData()
{
    if (this->blockUpdates || this->transformData == nullptr)
        return;

    TransformData& td = *this->transformData;

    if (checkFill->isChecked())
        td.fill = TransformData::Fill{(float)spinFillX->value(), (float)spinFillY->value(),
                                       (float)spinFillScaleX->value(), (float)spinFillScaleY->value(), false};
    else
        td.fill.reset();

    if (checkOpacity->isChecked())
        td.opacity = (float)spinOpacity->value();
    else
        td.opacity.reset();

    if (checkRotation->isChecked())
        td.rotation = (float)spinRotation->value();
    else
        td.rotation.reset();

    if (checkAnchor->isChecked())
        td.anchor = TransformData::Anchor{(float)spinAnchorX->value(), (float)spinAnchorY->value()};
    else
        td.anchor.reset();

    if (checkCrop->isChecked())
        td.crop = TransformData::Crop{(float)spinCropLeft->value(), (float)spinCropTop->value(),
                                       (float)spinCropRight->value(), (float)spinCropBottom->value()};
    else
        td.crop.reset();

    if (checkClip->isChecked())
        td.clip = TransformData::Clip{(float)spinClipX->value(), (float)spinClipY->value(),
                                       (float)spinClipW->value(), (float)spinClipH->value()};
    else
        td.clip.reset();

    if (checkBrightness->isChecked()) td.brightness = (float)spinBrightness->value(); else td.brightness.reset();
    if (checkContrast->isChecked()) td.contrast = (float)spinContrast->value(); else td.contrast.reset();
    if (checkSaturation->isChecked()) td.saturation = (float)spinSaturation->value(); else td.saturation.reset();
    if (checkVolume->isChecked()) td.volume = (float)spinVolume->value(); else td.volume.reset();

    if (checkEntrance->isChecked())
    {
        TransformData::Entrance e;
        e.property = comboEntranceProp->currentText();
        e.from = (float)spinEntranceFrom->value();
        e.to = (float)spinEntranceTo->value();
        e.duration = spinEntranceDuration->value();
        e.tween = comboEntranceTween->currentText();
        td.entrance = e;
    }
    else
        td.entrance.reset();
}
