// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimespanTest, "System.Core.Misc.Timespan", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTimespanTest::RunTest( const FString& Parameters )
{
	// constructors must create equal objects
	{
		FTimespan ts1_1 = FTimespan(3, 2, 1);
		FTimespan ts1_2 = FTimespan(0, 3, 2, 1);
		FTimespan ts1_3 = FTimespan(0, 3, 2, 1, 0);

		TestEqual(TEXT("Constructors must create equal objects (Hours/Minutes/Seconds vs. Days/Hours/Minutes/Seconds)"), ts1_1, ts1_2);
		TestEqual(TEXT("Constructors must create equal objects (Hours/Minutes/Seconds vs. Days/Hours/Minutes/Seconds/Milliseconds)"), ts1_1, ts1_3);
	}

	// component getters must return correct values
	{
		FTimespan ts2_1 = FTimespan(1, 2, 3, 4, 5, 6);

		TestEqual(TEXT("Component getters must return correct values (Days)"), ts2_1.GetDays(), 1);
		TestEqual(TEXT("Component getters must return correct values (Hours)"), ts2_1.GetHours(), 2);
		TestEqual(TEXT("Component getters must return correct values (Minutes)"), ts2_1.GetMinutes(), 3);
		TestEqual(TEXT("Component getters must return correct values (Seconds)"), ts2_1.GetSeconds(), 4);
		TestEqual(TEXT("Component getters must return correct values (Milliseconds)"), ts2_1.GetMilliseconds(), 5);
		TestEqual(TEXT("Component getters must return correct values (Microseconds)"), ts2_1.GetMicroseconds(), 6);
	}

	// durations of positive and negative time spans must match
	{
		FTimespan ts3_1 = FTimespan(1, 2, 3, 4, 5, 6);
		FTimespan ts3_2 = FTimespan(-1, -2, -3, -4, -5, -6);

		TestEqual(TEXT("Durations of positive and negative time spans must match"), ts3_1.GetDuration(), ts3_2.GetDuration());
	}

	// static constructors must create correct values
	{
		TestEqual(TEXT("Static constructors must create correct values (FromDays)"), FTimespan::FromDays(123).GetTotalDays(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromHours)"), FTimespan::FromHours(123).GetTotalHours(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromMinutes)"), FTimespan::FromMinutes(123).GetTotalMinutes(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromSeconds)"), FTimespan::FromSeconds(123).GetTotalSeconds(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromMilliseconds)"), FTimespan::FromMilliseconds(123).GetTotalMilliseconds(), 123.0);
		TestEqual(TEXT("Static constructors must create correct values (FromMicroseconds)"), FTimespan::FromMicroseconds(123).GetTotalMicroseconds(), 123.0);
	}

	// string conversions must return correct strings
	{
		FTimespan ts5_1 = FTimespan(1, 2, 3, 4, 5);

		TestEqual<FString>(TEXT("String conversion (Default)"), ts5_1.ToString(), TEXT("1.02:03:04.005"));
		TestEqual<FString>(TEXT("String conversion (%n%d.%h:%m:%s.%f)"), ts5_1.ToString(TEXT("%n%d.%h:%m:%s.%f")), TEXT("1.02:03:04.005"));
	}

	// parsing valid strings must succeed
	{
		FTimespan ts6_1 = FTimespan(1, 2, 3, 4, 5);
		FTimespan ts6_2;

		TestTrue(TEXT("Parsing valid strings must succeed (1.02:03:04.005)"), FTimespan::Parse(TEXT("1.02:03:04.005"), ts6_2));
		TestEqual(TEXT("Parsing valid strings must result in correct values (1.02:03:04.005)"), ts6_2, ts6_1);
	}

	// parsing invalid strings must fail
	{
		FTimespan ts7_1;

		//TestFalse(TEXT("Parsing invalid strings must fail (1,02:03:04.005)"), FTimespan::Parse(TEXT("1,02:03:04.005"), ts7_1));
		//TestFalse(TEXT("Parsing invalid strings must fail (1.1.02:03:04:005)"), FTimespan::Parse(TEXT("1.1.02:03:04:005"), ts7_1));
		//TestFalse(TEXT("Parsing invalid strings must fail (04:005)"), FTimespan::Parse(TEXT("04:005"), ts7_1));
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS