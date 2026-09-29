---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Test Service Library: TSL in CAPL Simulation Nodes
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CANoeCANalyzer/AutomatedTesting/TestServiceLibrary/TSLUsageCAPLSimulationNodes.htm)

[Test Service Library](../TestServiceLibrary.htm) Â» CAPL Â» Simulation Nodes

# Test Service Library: TSL in CAPL Simulation Nodes

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE

Test Service Library functions can also be used in CAPL simulation nodes.

However, it is simpler to use these functions in automated tests. For this reason, it is recommended to use automated tests to implement tests and use the Test Service Library functions.

## Configuration of Checks and Stimulus Generators

When Test Service Library functions are used in a simulation node, the Test Service Library must always be initialized by calling up [ChkConfig_Init](../../../CAPLFunctions/Test/Functions/CAPLfunctionChkConfigInit.htm) in the "on prestart" event procedure.

Checks and stimulus generators must always be created and parameterized before usage.

Checks and stimulus generators can already be created in "on prestart" or "on start". They can then be started, when they are needed, via the control functions.

## Starting and Controlling

The control functions are used to activate, deactivate, reset or delete a check or a generator. The testing or generation takes place in the background, in parallel to the continued processing of the simulation node.

Once a check or generator has been created, it can be used repeatedly during test execution, e.g., it can be started and stopped repeatedly. However, a check or stimulus generator which has already been started cannot be started again without being stopped first. You can also create several similar checks and use them simultaneously. A check or generator is identified by a unique number (ID), which is returned when the check or generator is created.

## Analyzing the Check Results

* Violations of check conditions, e.g., erroneous behavior of the SUT, are reported as defined by the user (this is implemented via the callback function).
* The automatically generated statistics can be accessed at any time during the runtime.  
Please note that the statistical data are not available in the "on StopMeasurement" event procedure.
* Querying the statistical data makes it possible to make a statement about the quality of the system being tested.

[Stimulus Functions Overview](../../../CAPLFunctions/Test/CAPLfunctionsTSLStimulusOverview.htm) â¢ [Sample Code (CAPL Simulation Node)](TSLCAPLSimulationNodeSampleCode.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)