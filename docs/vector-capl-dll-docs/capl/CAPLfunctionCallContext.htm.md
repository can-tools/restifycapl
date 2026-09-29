---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: callContext
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CAPLFunctions/DistributedObjects/Objects/CAPLfunctionCallContext.htm)

[CAPL Functions](../../CAPLfunctions.htm) Â» [Distributed Objects](../CAPLfunctionsDOOverview.htm) Â» callContext

# callContext

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

## Function Syntax

callContext \* <var>; // form 1

callContext <Method> <var>; // form 2

callContext <Prototype> <var>; // form 3

## [Method](../../../ProgrammingInterfaces/CAPL/General/ClassesAndObjects.htm) Syntax

* [callcontext::CreatePermanentHandle](../Methods/CAPLfunctionCallcontextCreatePermanentHandle.htm)
* [callcontext::DeferAnswer](../Methods/CAPLfunctionCallcontextDeferAnswer.htm)
* [callcontext::FromHandle](../Methods/CAPLfunctionCallcontextFromHandle.htm)
* [callcontext::ReleaseHandle](../Methods/CAPLfunctionCallcontextReleaseHandle.htm)
* [callcontext::ReturnCall](../Methods/CAPLfunctionCallcontextReturnCall.htm)
* [callcontext::SetDefaultAnswer](../Methods/CAPLfunctionCallcontextSetDefaultAnswer.htm)
* [callcontext::SetTimeToAnswer](../Methods/CAPLfunctionCallcontextSetTimeToAnswer.htm)

## Description

Contains data of a function call, in particular the parameter values.

## Parameters

| Method    | Method of a service, determining the data type of the call            |
| --------- | --------------------------------------------------------------------- |
| Prototype | Function prototype (signature), determining the data type of the call |

## Selectors

| Selector                                                                                                                                                                                                 | Type                            | Access Limitation           |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------- | --------------------------- |
| State<br>State of the call:<br>eCALLSTATE\_INITIAL: the call has not yet reached the providereCALLSTATE\_CALLED: the call has reached the provider, but the return has not yet reached the consumereCALLSTATE\_RETURNED: the return has reached the consumereCALLSTATE\_DISCARDED: the call context is invalid | enumeration                     | Read only                   |
| Side<br>Shows where the call is currently being handled:<br> eCOSIDE\_CONSUMER: consumer sideeCOSIDE\_PROVIDER: provider side                                                                            | enumeration                     | Read only                   |
| Consumer<br>Name of the consumer which started the call.                                                                                                                                                 | char[]                          | Read only                   |
| Provider<br>Name of the provider which is called.                                                                                                                                                        | char[]                          | Read only                   |
| CallTime<br>Point of time (in simulation time, in nanoseconds) when the call was started (on consumer side) or the call reached the provider (on provider side).                                         | int64                           | Read only                   |
| ReturnTime<br>Point of time (in simulation time, in nanoseconds) when the call was returned to the consumer (on provider side) or the return reached the consumer (on consumer side).                    | int64                           | Read only                   |
| ReqID<br>A request ID which identifies the call. Can be used to match replies to calls if several calls are waiting for a reply. The request ID is internal and not necessarily transmitted on the network. | int64                           | Read only                   |
| <Parameter Name><br>Value of the named parameter. Note that parameters are not accessible or read-only depending on the side and state of the call, e.g. you cannot access out-parameters in state Initial and cannot change in-parameters in state Returned. | <data type of the parameter>    | <depends on side and state> |
| Result<br>Return value of the function (if the function return type is not void).                                                                                                                        | <data type of the return value> | <depends on side and state> |

## Example

| ![Example](../../../../Resources/vImages/vExample.png "Example") | variables   {    callContext MirrorAdjustment.Adjust deferredAnswer;   }    on fct\_Calling MirrorAdjustment[all, LeftMirror].Adjust   {    this.DeferAnswer();    deferredAnswer = this;   }    on sysvar Panels::ReturnAnswer   {    deferredAnswer.ReturnCall(Mirrors::AdjustResult::OK);   } |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Programming with the Communication Concept (C#, Python and CAPL)](../../../CANoeCANalyzer/CommunicationConcept/Programming/CCP.htm) 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)