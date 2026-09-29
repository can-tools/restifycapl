---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Enumeration Types
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/EnumerationTypes.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Enumeration Types

# Enumeration Types

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>Enumeration types are not compatible for use with older versions of your CANoe. Therefore, they can only be used in CAPL programs from version 7.0 or higher. |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

Enumeration types are defined in CAPL in exactly the same way as in C:

enum Colors { Red, Green, Blue };

Element names must be unique throughout the CAPL program.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>They might hide the names of messages or signals defined in a database. |
| ------------------------------------------------------- | ------------------------------------------------------------------------------- |

Fixed integer values can also be assigned to individual elements:

enum State { State_Off = -1, State_On = 1 };

If no values are assigned, the first element will have the value 0 and all subsequent elements will take the previous value plus 1.

Enumeration types can be defined wherever variables can be declared.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>Since version 7.1 you can get the value identifier with the name method. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------- |

## Semantics

The following semantics exist for enumeration types in CAPL:

* If necessary, an enumeration type is implicitly converted to an integer type (e.g. with arithmetical operations):

    int i; enum State state;  
state = State_Off;  
i = state + 1; // i == 0

* Enumeration types can be used especially in switch-case-statement:

    switch (state) {  
case State_On: write("On"); break;  
case State_Off: write("Off"); break;  
default: write("Undefined"); break;  
}

* For a conversion from an integer type to an enumeration type or between two different enumeration types a cast is necessary:

    enum State state; enum Colors color;  
state = (enum State) 1; // state == State_On  
color = (enum Color) state; // color == Green

* You can get the value identifier with the name method:

    write("Color is %s", color.name());  
on signal Active  
{  
write("Value: %s", ((enum VtSig_Switch)$Active).name());  
}

    If no identifier for the current value is available, the value will be converted to a string.

* You can use the containsValue method to check whether an enumeration type contains a particular value:

    if (color.containsValue(x)) write("The name of value %d is %s", x, ((enum Colors)x).name());

## Enumerations from Databases

Valid for CANoe DE,

Value tables in DBC databases automatically define enumeration types in CAPL. The type takes the name of the value table and its elements come from the same table.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>Value table called VtSig\_Gear containing elements Idle, Gear\_1, Gear\_2, etc.<br>variables {    enum VtSig\_Gear oldGear = Idle;   }   on signal Gear    {    if (abs(this â oldGear) > 0 && this != Idle)    {    write("Jump in Gear!");    oldGear = (enum VtSig\_Gear) this;    }   } |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>An element from a value table assigned to multiple values in the database cannot be used as an enumeration element.An element from a value table whose name is not a valid CAPL name cannot be used.The names of types and variables in the CAPL program mask the names of the value table and its elements.   However, you can add the name of the value table to elements: gear = VtSig\_Gear::Idle; |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Enumerations for System Variables

Additionally value tables of system variables automatically define enumeration types. The type has the prefix VtSv_ and as next consists of the namespace and the name of the system variable; wherein the single parts are connected by underscores. The descriptions may be attached as constants; if necessary they have to be qualified with namespace and name of the variable.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>variables {     enum VtSv\_Osek\_NMState nmState = NM\_On;    }<br>on key 't' {    @sysvar::Osek::NMState = sysvar::Osek::NMState::NM\_BusSleep;   } |
| ---------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Enumerations for variables in CAPL

You can either declare variables associated with an enumeration type directly when defining the type (in which case you can leave out the name of the type) or you can reference the type later by means of its name.

Enumeration types can also be used as parameters and return types in functions. To do this, you must add the [keyword](Keywords.htm) enum.

variables  
{  
enum { Apple, Pear, Banana } fruit = Apple;  
enum Colors { Red, Green, Blue };  
enum Colors color;  
}  

enum Colors NextColor(enum Colors c)  
{  
if (c == Blue) return Red;  
else return (enum Colors) (c + 1);  
}

## Enumerations for Bus Systems

Valid for CANoe DE,

There is a predefined enumeration type for bus systems. It is equivalent to the CAPL definition

enum BusType  
{  
eCAN = 1,  
eFlexRay = 7,  
eEthernet = 11,  
eAfdx = 16,  
eWildcard = 0xFFFFFFFF  
};

This enumeration type is used especially for the BusType selector of the [PDU objects](../../../CAPLFunctions/Other/Objects/CAPLfunctionPDU.htm).

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)