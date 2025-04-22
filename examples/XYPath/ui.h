#pragma

#include "fl/ui.h"

using namespace fl;

UITitle title("XYPath Demo");
UIDescription description("Use a path on the WaveFx");

UICheckbox useWaveFx("Use WaveFX", true);
UISlider transition("Transition", 0.0f, 0.0f, 1.0f, 0.01f);
UIButton button("Trigger");
UISlider scale("Scale", 1.0f, 0.0f, 1.0f, 0.01f);
UISlider speed("Speed", 1.0f, 0.25f, 20.0f, 0.01f);
UISlider numberOfSteps("Number of Steps", 32.0f, 1.0f, 100.0f, 1.0f);

UICheckbox advancedFrame("Advanced Frame", true);
UIButton advancedFrameButton("Advanced Frame Button");