#ifndef __CPG_TYPES_H__
#define __CPG_TYPES_H__

#include "cpg-property.h"

typedef struct _CpgInstruction 		CpgInstruction;

/**
 * CpgInstructionType:
 * @CPG_INSTRUCTION_TYPE_NONE: none
 * @CPG_INSTRUCTION_TYPE_FUNCTION: function
 * @CPG_INSTRUCTION_TYPE_NUMBER: number
 * @CPG_INSTRUCTION_TYPE_OPERATOR: operator
 * @CPG_INSTRUCTION_TYPE_PROPERTY: property 
 *
 * Enum used to indicate instruction type
 *
 **/
typedef enum {
	CPG_INSTRUCTION_TYPE_NONE,
	CPG_INSTRUCTION_TYPE_FUNCTION,
	CPG_INSTRUCTION_TYPE_NUMBER,
	CPG_INSTRUCTION_TYPE_OPERATOR,
	CPG_INSTRUCTION_TYPE_PROPERTY
} CpgInstructionType;

struct _CpgInstruction
{
	CpgInstructionType type;
};

typedef struct
{
	CpgInstruction parent;
	
	guint id;
	gchar *name;
	gint arguments;
	gint vargs;
} CpgInstructionFunction;

typedef struct
{
	CpgInstruction parent;
	
	gdouble value;
} CpgInstructionNumber;

typedef struct
{
	CpgInstruction parent;

	CpgProperty *property;
} CpgInstructionProperty;

#endif /* __CPG_TYPES_H__ */

