#ifndef __CPG_PROPERTY_H__
#define __CPG_PROPERTY_H__

struct _CpgExpression;

typedef struct 
{
	char *name;
	struct _CpgExpression *value;
	double update;
	struct _CpgExpression *initial;
	char integrated;
} CpgProperty;

CpgProperty 	*cpg_property_new	(char const *name, char const *expression, char integrated);
void			 cpg_property_free	(CpgProperty *property);

#endif /* __CPG_PROPERTY_H__ */
