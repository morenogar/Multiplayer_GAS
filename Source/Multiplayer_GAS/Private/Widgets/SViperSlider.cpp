#include "Widgets/SViperSlider.h"

#include "Misc/App.h"


SViperSlider::SViperSlider()
	: bUsingGamepad(false)
	, bWasInputting(false)
{
}

FReply SViperSlider::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bUsingGamepad)
	{
		return FReply::Handled();
	}

	return SSlider::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SViperSlider::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bUsingGamepad)
	{
		return SSlider::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

	return FReply::Unhandled();
}

FReply SViperSlider::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (IsLocked())
		return SWidget::OnAnalogValueChanged(MyGeometry, InAnalogInputEvent);

	FReply Reply = FReply::Unhandled();
	const FKey KeyPressed = InAnalogInputEvent.GetKey();

	// Validar orientación
	if ((Orientation == EOrientation::Orient_Horizontal && KeyPressed != EKeys::Gamepad_LeftX) || 
		(Orientation == EOrientation::Orient_Vertical && KeyPressed != EKeys::Gamepad_LeftY))
	{
		return SSlider::OnAnalogValueChanged(MyGeometry, InAnalogInputEvent);
	}

	Reply = FReply::Handled();

	if (bUsingGamepad == false)
		return Reply;

	const float AnalogValue = InAnalogInputEvent.GetAnalogValue();
	const float AbsAnalogValue = FMath::Abs(AnalogValue);

	// Umbral mínimo para evitar el "drift" del mando
	constexpr float AnalogStepThreshold = 0.5f;

	if (AbsAnalogValue < AnalogStepThreshold)
	{
		bWasInputting = false;
		return Reply;
	}

	// Si es la primera vez que detectamos el input en este ciclo de pulsación
	if (bWasInputting == false)
	{
		// Opcional: Podrías resetear LastAnalogStepTime aquí a 0 si quieres que 
		// el primer movimiento sea instantáneo al tocar el stick.
		bWasInputting = true;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	const double TimeSinceLastStep = CurrentTime - LastAnalogStepTime;

	// Usamos directamente el StepTime definido
	const float TargetStepTime = StepTime.Get();

	// Salir si no ha pasado suficiente tiempo desde el último incremento
	if (TimeSinceLastStep < TargetStepTime)
		return Reply;

	const float StepSizeValue = StepSize.Get();

	// Calculamos el nuevo valor
	float NewValue = GetValue();
	NewValue += StepSizeValue * FMath::Sign(AnalogValue);

	// Aplicar y notificar
	CommitValue(FMath::Clamp(NewValue, MinValue, MaxValue));
	OnAnalogCapture.ExecuteIfBound(GetValue());

	// Actualizar el marcador de tiempo
	LastAnalogStepTime = CurrentTime;

	return Reply;
}

FNavigationReply SViperSlider::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	const EUINavigation NavType = InNavigationEvent.GetNavigationType();
	
	if ((Orientation == EOrientation::Orient_Horizontal && NavType != EUINavigation::Left && NavType != EUINavigation::Right) ||
		(Orientation == EOrientation::Orient_Vertical && NavType != EUINavigation::Down && NavType != EUINavigation::Up))
		return SSlider::OnNavigation(MyGeometry, InNavigationEvent);

	if (IsLocked())
		return FNavigationReply::Stop();

	const float StepDir = (NavType == EUINavigation::Left || NavType == EUINavigation::Down) ? -1.0f : 1.0f;

	CommitValue(FMath::Clamp(GetValue() + GetStepSize() * StepDir, MinValue, MaxValue));
	OnAnalogCapture.ExecuteIfBound(GetValue());

	return FNavigationReply::Explicit(nullptr);
}

void SViperSlider::Construct(const FArguments& InArgs)
{
	SSlider::Construct(SSlider::FArguments()
    		.Style(InArgs._Style)
    		.IsFocusable(InArgs._IsFocusable)
    		.OnMouseCaptureBegin(InArgs._OnMouseCaptureBegin)
    		.OnMouseCaptureEnd(InArgs._OnMouseCaptureEnd)
    		.OnControllerCaptureBegin(InArgs._OnControllerCaptureBegin)
    		.OnControllerCaptureEnd(InArgs._OnControllerCaptureEnd)
    		.OnValueChanged(InArgs._OnValueChanged));
	
	OnAnalogCapture = InArgs._OnAnalogCapture;
	StepTime = InArgs._StepTime;

}

void SViperSlider::SetUsingGamepad(const bool InValue)
{
	bUsingGamepad = InValue;
}

void SViperSlider::SetStepTime(TAttribute<float> InStepSize)
{
	StepTime = MoveTemp(InStepSize);
}
