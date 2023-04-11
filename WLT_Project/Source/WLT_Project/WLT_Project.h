// Copyright Notice: Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * This is the header file for the WLT_Project class.
 * It includes any necessary headers and declares any classes, functions, and variables used in the class.
 */
 
class WLT_PROJECT_API WLT_Project
{
public:
	// Default constructor
	WLT_Project();

	// Default destructor
	~WLT_Project();

	// Public member function declarations
	void StartGame();
	void EndGame();

private:
	// Private member variable declarations
	int32 Score;
};
