---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Structures
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/Structures.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Structures

# Structures

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

Structs group elements (member). Structured types can be declared in CAPL in a similar way to C:

struct Data {  
int type;  
long l;  
char name[50];  
};

The name of the structure must be unique in the program. Simple data types, enumeration data types, other structures and fields of these types can be used as elements (members) within the structure.

Structure types can be defined wherever variables can also be declared.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>The use of self-defined structures is not compatible with older versions of your CANoe. For this reason they may only be used in CAPL programs from version 7.0 Service Pack 3. |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Variables

You can declare variables of a structure type either directly during definition of the type (in this case the name of the type can also be omitted) or you can reference the type later using its name:

variables  
{  
struct Point  
{  
int x;  
int y;  
};  
struct Point myPoint;  
}

## Code Alternative to the Definition of Structures (Structs)

* Code Alternative 1

It is possible to define an unnamed Struct and to simultaneously assign this Struct as a data type to a variable (pair).

variables  
{  
struct { int first; int second; } pair;  
}  

on start  
{  
pair.first = 1;  
pair.second = 2;  
}

* Code Alternative 2

It is possible to define a Struct and to simultaneously assign this Struct as a data type to a variable (pair). You can additionally assign this data type to another variable (pair2).

variables  
{  
struct PairStructType { int first; int second; } pair;  
struct PairStructType pair2;  
}  

on start  
{  
pair.first = 1;  
pair.second = 2;  
}

You can initialize the elements of the structure directly during variable declaration. You can â but need not to â name the single elements explicitly. If you don't name them, the order of the structure definition is used:

variables  
{  
struct Point myPoint = { x = 0.5, y = 3.8 };  
struct Point myPoint2 = { 0.5, 3.8 };  
}

If you do not initialize one or more elements, they will be automatically given the value 0.

Structures can be used as parameters of functions, whereby a reference is transferred automatically. In this case the [keyword](Keywords.htm) struct must be included:

void process(struct Data data)

Structures are not allowed as return values of functions. However, you can also declare fields of structures:

variables  
{  
struct Point  
{  
int x;  
int y;  
};  
struct Point myPoint;  
struct Point allPoints[50];  
}

Elements of a structure are accessed with '.' as in C:

on start  
{  
myPoint.x = 7;  
myPoint.y = 2;  

allPoints[3].x = 1;  
allPoints[3].y = 5;  
}

## Byte access

For self-defined structures there are no comparison or assignment operators. However, you can access the data bytes of a structure directly with a number of functions:

* [memcpy](../../../CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCpy.htm)
* [memcpy_h2n](../../../CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCpyH2n.htm)
* [memcpy_n2h](../../../CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCpyN2h.htm)
* [memcmp](../../../CAPLFunctions/StructByteAccess/Functions/CAPLfunctionMemCmp.htm)

## Alignment

For structures you can exactly specify the positions of the various elements in memory, or to be more accurate, their so-called alignment relative to specific limits. To do this, write the following before declaration of the structure type _align:

_align(2) struct Point2 { // Struct with element alignment of 2 bytes

For alignment only the values 1, 2, 4, and 8 are allowed. If no alignment is specified, an alignment of 8 will be assumed. In addition, the following rules apply:

* The alignment of simple data types is their size (in bytes). The data type DWORD, for example, has an alignment of 4.
* The alignment of a field is equal to the alignment of its elements.
* The alignment of a structure element is the minimum of the alignment of its data type and the alignment specified for the structure.
* The alignment of a structure itself is the maximum of the alignment of its elements.

The first element of a structure is always at a distance (offset) of 0 bytes from the start of the structure. The following applies to further elements: Their offset is the smallest multiple of their alignment that is greater or equal to the offset of the previous element plus the size of the previous element.

The size of a structure is simply the offset of its last element plus its size.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>struct Point { // note: default \_align(8)    byte x; // offset 0, size 1    byte y; // alignment 1, offset 1, size 1, padding before: 0    }; // size 2, alignment (of the struct) 1<br>struct LongPoint { // note: default \_align(8)   byte x; // offset 0, size 1   qword y; // alignment 8, offset 8, size 8, padding before: 7   }; // size 16, alignment (of the struct) 8<br>\_align(2) struct Point2 {   byte x; // offset 0, size 1, (alignment 1)   qword y; // alignment 2, offset 2, size 8, padding before: 1   }; // size 10, alignment (of the struct) 2<br>struct Points { // note: \_align(8) per default   struct Point p1; // offset 0, size 2, (alignment 1)   byte x; // alignment 1, offset 2, size 1, padding before: 0   struct Point2 p2; // alignment 2, offset 4, size 10, padding before: 1   }; // size 14, alignment (of the struct) 2 |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Information about structures

The following functions return information about size and alignment of the structure as well as the offset of the element (member):

| Function                            | Description                                        |
| ----------------------------------- | -------------------------------------------------- |
| \_\_size\_of(aligned-type)          | Returns the size of a structure                    |
| \_\_alignment\_of(aligned-type)     | Returns the alignment of a structure.              |
| \_\_offset\_of(struct-type, member) | Returns the offset of a member within a structure. |

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>struct Points { // note: \_align(8) per default    Point p1; // offset 0, size 2, (alignment 1)    byte x; // alignment 1, offset 2, size 1, padding before: 0    Point2 p2; // alignment 2, offset 4, size 10, padding before: 1    }; // size 14, alignment (of the struct) 2<br>\_\_size\_of(struct Points) // returns 14   \_\_alignment\_of(struct Points) // returns 2   \_\_offset\_of(struct Points, p1) // returns 0   \_\_offset\_of(struct Points, x) // returns 2   \_\_offset\_of(struct Points, p2) // returns 4 |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Data types for Variables](DataTypesForVariables.htm) â¢ [Data types for Function Parameters](DataTypesForFunctionParameters.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)