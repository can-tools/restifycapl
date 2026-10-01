---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: on timer
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CAPLFunctions/Other/EventProcedures/CAPLfunctionOnTimer.htm)

[CAPL Functions](../../CAPLfunctions.htm) Â» [General](../CAPLGeneralStartPage.htm) Â» [Event Procedures](../CAPLfunctionsEventProceduresOverview.htm) Â» on timer

# on timer

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

You can define time events in CAPL. When this event occurs, i.e. when a certain period of time elapses, the associated on timer procedure is called. You can program cyclic program sequences by resetting the same time event within the on timer procedure.

The timer variable can be accessed with the key word [this](CAPLfunctionKeywordThis.htm) within the event procedure.

You would start a previously-defined timer with the function [setTimer](../Functions/CAPLfunctionSetTimer.htm).

In CAPL exists the following variable types for timer:

* timer - timer based on seconds
* msTimer - timer based on milliseconds

After the timer has elapsed, the associated on timer procedure is called. The maximum time is 2147483647 s (=596523.23h) for variables of the type timer and 2147483647 ms (= 2147483,647 s = 596,52h) for variables of the type msTimer. With the function [cancelTimer](../Functions/CAPLfunctionCancelTimer.htm) you can stop a timer which has already been started and thereby prevent the associated on timer procedure from being called.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>If several CAPL timers elapse to the exact same time:<br>all timers will be executedthe event procedures of these timers will be operated in an undefined sequence |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>After pressing key 'a' in the following example timer is set and after 20 milliseconds a timer callback is raised:<br>msTimer myTimer;   message 100 msg;   ...   on key 'a' {    setTimer(myTimer,20);   }   ...   on timer myTimer   {     output(msg);   } |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## [ClosedVersion Extensions](javascript:void(0))

* Version 7.2

Since version 7.2, you can retrieve the name of the timer as a string constant with this.name.

* Version 7.5

Since version 7.5 you can save timers in an array.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>variables   {    mstimer myTimers[10];   } |
| ---------------------------------------------------------------- | ----------------------------------------------------- |

You have to set each single timer but the same timer procedure will be called for all timers. For that the procedure has to contain a parameter (dword type) that indicates the index of the just run out timer.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>on start   {    dword i;    for (i = 0; i < elcount(myTimers); ++i)    myTimers[i].set(100 + 20 \* i);   }    on timer myTimers(dword index)   {    // ...   } |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

In the procedure the keyword this describes the whole array. E.g. if you want to set the just run out timer, you have to add an index to this.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>write("Timer %s with index %d fired", this[index].name, index);   setTimer(this[index], 2000); |
| ---------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- |

[Class: Timer, MsTimer](../../ObjectOrientedProg/CAPLfunctionsOOPTimer.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)