---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Test Service Library in Automated CAPL Tests
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CANoeCANalyzer/AutomatedTesting/TestServiceLibrary/TSLUsageAutomatedCAPLTests.htm)

[Test Service Library](../TestServiceLibrary.htm) Â» CAPL Â» Automated Testing

# Test Service Library in Automated CAPL Tests

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

## Checks

Checks are used as constraints or conditions in automated tests. The checks must merely be created, parameterized and activated for this purpose, and activated/deactivated as a constraint/condition:

dword id;  
id = ChkStart_MsgAbsCycleTimeViolation (MsgSpeed, 80, 120);  
TestAddConstraint (id);  
...  
TestRemoveConstraint (id);  
ChkControl_Destroy (id);

It is not necessary to use callback functions to react to errors or to control the check via a CAPL function, as constraints and conditions automatically influence the test result and document it in the test report.

Once a check or generator has been created, it can be used repeatedly during test execution, e.g., it can be started and stopped repeatedly. However, a check or stimulus generator which has already been started cannot be started again without being stopped first. It is generally advisable to use a ChkControl_Reset call prior to reuse as a constraint or condition. You can also create several similar checks and use them simultaneously. A check or generator is identified by a unique number (ID), which is returned when the check or generator is created.

In principle, it is possible to use a check in an automated CAPL test in the same way as in a [CAPL simulation node](TSLUsageCAPLSimulationNodes.htm). However, this is generally more time-consuming and is not recommended.

## Stimulus Generators

Stimulus generators, like checks, must be created and parameterized prior to usage. You would, for example, specify the sink for the value series and the cycle time of the generated value series. The control functions are used to activate, deactivate, reset or delete a generator. These control functions immediately roll back to the calling function, e.g., they do not wait. The generation of value series takes place in the background, in parallel to the continued test processing.

dword rampId;  
rampId = StmCreate_Ramp (StmMsg, StmMsg::RampSig, -10, 10, 5, 300, 200, 100, 400);  
StmControl_Start (rampId);  
WaitForTimeout (1000);  
StmControl_Stop (rampId);

In this example, the generation of a ramp-formed value series onto the RampSig signal of the StmMsg message is created and started. The generator is stopped again after 1000 ms.

[Test Feature Set (TFS)](../TestFeatures.htm) â¢ [Structure of CAPL test modules](../TestModules/CAPLTestModuleStructure.htm) â¢ [Constraints and conditions (CAPL)](../TestModules/CAPLConstraintsConditions.htm) â¢ [Stimulus functions](../../../CAPLFunctions/Test/CAPLfunctionsTSLStimulusOverview.htm) â¢ [Check functions](../../../CAPLFunctions/Test/CAPLfunctionsTSLCheckOverview.htm) â¢ [Sample Code (CAPL test](TSLCAPLTestModuleSampleCode.htm)[module/](TSLCAPLTestModuleSampleCode.htm)[unit)](TSLCAPLTestModuleSampleCode.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)