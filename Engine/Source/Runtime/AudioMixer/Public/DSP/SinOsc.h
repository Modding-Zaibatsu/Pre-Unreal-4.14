// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Audio
{
	/** 
	* FOsc
	* Direct-form sinusoid oscillator. 
	* Created with a biquad filter (using only feedback coefficients) with poles directly on unit circle in z-plane.
	* Setting frequency uses current filter state to compute initial conditions to avoid pops when changing frequency.
	*/
	class FSineOsc
	{
	public:
		/** Constructor */
		FSineOsc();

		/** Non-default constructor */
		FSineOsc(const int32 InSampleRate, const float InFrequencyHz, const float Scale = 1.0f, const float Add = 0.0f);

		/** Virtual destructor */
		virtual ~FSineOsc();

		/** Initialize the oscillator with a sample rate and new frequency. Must be called before playing oscillator. */
		void Init(const int32 InSampleRate, const float InFrequencyHz, const float Scale = 1.0f, const float Add = 0.0f);

		/** Sets the frequency of the oscillator in Hz (based on sample rate). Performs initial condition calculation to avoid pops. */
		void SetFrequency(const float InFrequencyHz);

		/** Returns the current frequency. */
		float GetFrequency() const;

		/** Generates the next sample of the oscillator. */
		float operator()();

	protected:
		int32 SampleRate;
		float FrequencyHz;
		float B1;	// Biquad feedback coefficients
		float B2;
		float Yn_1;	// y(n - 1)
		float Yn_2;	// y(n - 2)
		float Scale;
		float Add;
	};


}
