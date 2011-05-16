%{

#include "cpg-parser-context.h"
#include "cpg-parser.h"

static void cpg_parser_error (YYLTYPE *locp, CpgParserContext *context, char *s);
int cpg_parser_lex(YYSTYPE *lvalp, YYLTYPE *llocp, void *scanner);

#define scanner (cpg_parser_context_get_scanner (context))

#define append_array(array, type, value, code)				\
{									\
	GArray *arret = array;						\
	type theval = value;						\
									\
	if (arret == NULL)						\
	{								\
		arret = g_array_new (FALSE, FALSE, sizeof (type));	\
	}								\
									\
	g_array_append_val (arret, theval);				\
									\
	code;								\
}

#define errb								\
	if (cpg_parser_context_get_error (context) != NULL)		\
	{								\
		YYERROR;						\
	}

static CpgFunctionPolynomialPiece *create_polynomial_piece (gdouble  start,
                                                            gdouble  end,
                                                            GArray  *coefficients);

static CpgFunctionArgument *create_function_argument (gchar const *name,
                                                      gboolean     is_optional,
                                                      gdouble      default_value);

%}

%token T_KEY_IN T_KEY_INTEGRATED T_KEY_ONCE T_KEY_OUT

%token T_KEY_STATE T_KEY_LINK T_KEY_NETWORK T_KEY_FUNCTION T_KEY_INTERFACE T_KEY_IMPORT T_KEY_INPUT_FILE T_KEY_POLYNOMIAL T_KEY_FROM T_KEY_TO T_KEY_PIECE T_KEY_TEMPLATES T_KEY_ATTACH T_KEY_APPLY T_KEY_DEFINE T_KEY_BIDIRECTIONAL T_KEY_ALL T_KEY_REMOVE T_KEY_INTEGRATOR

%token <numf> T_DOUBLE
%token <numf> T_INTEGER
%token <id> T_IDENTIFIER
%token <id> T_REGEX
%token <id> T_STRING

%type <flags> flags
%type <flags> flag

%type <id> nth
%type <id> selector_pseudo_identifier

%type <property> property_def
%type <array> double_list
%type <piece> polynomial_piece
%type <array> polynomial_pieces
%type <array> function_argument_list
%type <array> template_list
%type <array> selector_list
%type <argument> function_argument
%type <selector> selector
%type <num> directional
%type <num> all

%type <object> scope_end
%type <object> link
%type <object> state
%type <id> identifier_or_string
%type <id> expanded_string

%define api.pure
%name-prefix="cpg_parser_"

%parse-param {CpgParserContext *context}
%lex-param {void *scanner}
%error-verbose

%locations
%defines

%union
{
	char *id;
	CpgProperty *property;
	CpgPropertyFlags flags;
	gdouble numf;
	gint num;
	GSList *list;
	GArray *array;
	CpgFunctionPolynomialPiece *piece;
	CpgFunctionArgument *argument;
	CpgObject *object;
	CpgSelector *selector;
}

%start document

%destructor { cpg_parser_context_pop_scope (context); } scope_start
%destructor { cpg_parser_context_pop_scope (context); } state_scope_start
%destructor { cpg_parser_context_pop_scope (context); } link_scope_start
%destructor { cpg_parser_context_pop_scope (context); } network_scope_start

%destructor { cpg_parser_context_pop_template (context); } template_scope_start

%expect 5

%%

document
	:
	| document toplevel
	;

toplevel
	: network
	| state
	| link
	| function
	| polynomial
	| coupling
	| import
	| apply_template
	| templates
	| define
	| remove
	;

remove
	: T_KEY_REMOVE selector_list	{ cpg_parser_context_remove (context, $2); }
	;

network
	: T_KEY_NETWORK network_scope_start network_contents scope_end
	;

network_contents
	:
	| network_contents property
	| network_contents integrator
	;

integrator
	: T_KEY_INTEGRATOR '=' expanded_string	{ cpg_parser_context_set_integrator (context, $3); }
	;

define
	: T_KEY_DEFINE T_IDENTIFIER '=' expanded_string { cpg_parser_context_define (context, $2, $4); }
	;

expanded_string
	: T_STRING			{ $$ = cpg_parser_context_expand_defines (context, $1); }
	;

templates
	: T_KEY_TEMPLATES template_scope_start template_contents template_scope_end
	;

template_scope_end
	: '}'				{ cpg_parser_context_pop_template (context); }
	;

template_scope_start
	: '{'				{ cpg_parser_context_push_template (context); }
	;

template_item
	: state
	| link
	| import
	| apply_template
	;

template_contents
	:
	| template_contents template_item;
	;

identifier_or_string
	: T_IDENTIFIER
	| expanded_string
	;

network_scope_start
	: '{'				{ cpg_parser_context_push_scope (context,
					                                 CPG_PARSER_CONTEXT_SCOPE_NETWORK); errb }
	;

state
	: T_KEY_STATE identifier_or_string state_scope_start { cpg_parser_context_set_id (context, $2, NULL); } state_contents scope_end { $<object>$ = $6; }
	| T_KEY_STATE identifier_or_string ':' template_list state_scope_start { cpg_parser_context_set_id (context, $2, $4); } state_contents scope_end { $<object>$ = $8; }
	;

state_scope_start
	: '{'				{ cpg_parser_context_push_scope (context,
					                                 CPG_PARSER_CONTEXT_SCOPE_STATE); errb }
	;

scope_start
	: '{'				{ cpg_parser_context_push_scope (context,
					                                 CPG_PARSER_CONTEXT_SCOPE_NONE); errb }
	;

scope_end
	: '}' 				{ $$ = cpg_parser_context_pop_scope (context); errb }
	;

link
	: T_KEY_LINK identifier_or_string link_scope_start { cpg_parser_context_set_id (context, $2, NULL); } link_contents scope_end { $<object>$ = $6; } connect_link
	| T_KEY_LINK identifier_or_string ':' template_list link_scope_start { cpg_parser_context_set_id (context, $2, $4); } link_contents scope_end { $<object>$ = $8; } connect_link
	;

connect_link
	:
	| directional T_KEY_FROM selector T_KEY_TO selector
					{ cpg_parser_context_link_one (context,
					                               CPG_LINK ($<object>0),
					                               $3,
					                               $5,
					                               $1,
					                               FALSE); errb }

link_scope_start
	: '{'				{ cpg_parser_context_push_scope (context,
					                                 CPG_PARSER_CONTEXT_SCOPE_LINK); errb }
	;

function
	: T_KEY_FUNCTION T_IDENTIFIER '(' function_argument_list ')' scope_start expanded_string scope_end
					{ cpg_parser_context_add_function (context, $2, $7, $4); errb }
	;

polynomial
	: T_KEY_POLYNOMIAL T_IDENTIFIER '(' ')' scope_start polynomial_pieces scope_end
					{ cpg_parser_context_add_polynomial (context, $2, $6); errb }
	;

polynomial_pieces
	: 				{ $$ = NULL; }
	| polynomial_pieces polynomial_piece
					{ append_array ($1, CpgFunctionPolynomialPiece *, $2, $$ = arret); }
	;

polynomial_piece
	: T_KEY_PIECE T_KEY_FROM T_DOUBLE T_KEY_TO T_DOUBLE '=' double_list
					{ $$ = create_polynomial_piece ($3, $5, $7); }
	;

double_list
	: T_DOUBLE			{ append_array (NULL, gdouble, $1, $$ = arret); }
	| T_INTEGER			{ append_array (NULL, gdouble, $1, $$ = arret); }
	| double_list ',' T_DOUBLE	{ append_array ($1, gdouble, $3, $$ = arret); }
	| double_list ',' T_INTEGER	{ append_array ($1, gdouble, $3, $$ = arret); }
	;

template_list
	: identifier_or_string		{ append_array (NULL, gchar *, g_strdup ($1), $$ = arret); }
	| template_list ',' identifier_or_string
					{ append_array ($1, gchar *, g_strdup ($3), $$ = arret); }
	;

selector_list
	: selector			{ append_array (NULL, CpgSelector *, $1, $$ = arret); }
	| selector_list ',' selector	{ append_array ($1, CpgSelector *, $3, $$ = arret); }
	;

function_argument_list
	: function_argument		{ append_array (NULL, CpgFunctionArgument *, $1, $$ = arret); }
	| function_argument_list ',' function_argument
					{ append_array ($1, CpgFunctionArgument *, $3, $$ = arret); }
	;

function_argument
	: T_IDENTIFIER '=' T_DOUBLE	{ $$ = create_function_argument ($1, TRUE, $3); }
	| T_IDENTIFIER			{ $$ = create_function_argument ($1, FALSE, 0.0); }
	;

state_contents
	:
	| state_contents property
	| state_contents state
	| state_contents link
	| state_contents interface
	| state_contents coupling
	;

interface
	: T_KEY_INTERFACE '{' interface_contents '}'
	;

interface_contents
	:
	| interface_contents interface_item
	;

interface_item
	: T_IDENTIFIER '=' selector	{ cpg_parser_context_add_interface (context, $1, $3); errb }
	;

link_contents
	:
	| link_contents action
	| link_contents property
	;

property
	: flags property_def		{ cpg_property_set_flags ($2, $1); }
	| property_def
	;

property_def
	: T_IDENTIFIER '=' expanded_string
					{ $$ = cpg_parser_context_add_property (context, $1, $3); errb }
	;

flags
	: 				{ $$ = 0; }
	| flags flag 			{ $$ = $1 | $2; }
	;

flag
	: T_KEY_IN			{ $$ = CPG_PROPERTY_FLAG_IN; }
	| T_KEY_OUT			{ $$ = CPG_PROPERTY_FLAG_OUT; }
	| T_KEY_INTEGRATED		{ $$ = CPG_PROPERTY_FLAG_INTEGRATED; }
	| T_KEY_ONCE			{ $$ = CPG_PROPERTY_FLAG_ONCE; }
	;

action
	: T_IDENTIFIER '<' expanded_string
					{ cpg_parser_context_add_action (context, $1, $3); errb }
	;

selector
	: selector_pseudo nested_selector
					{ $$ = cpg_parser_context_pop_selector (context); errb }
	| selector_identifier nested_selector
					{ $$ = cpg_parser_context_pop_selector (context); errb }
	| selector_regex nested_selector
					{ $$ = cpg_parser_context_pop_selector (context); errb }
	;

selector_identifier
	: identifier_or_string 		{ cpg_parser_context_push_selector (context, $1); errb }
	;

selector_regex
	: T_REGEX 			{ cpg_parser_context_push_selector_regex (context, $1); errb }
	;

nested_selector
	:
	| nested_selector '.' selector_identifier
	| nested_selector '.' selector_regex
	| nested_selector selector_pseudo
	;

nth
	: T_INTEGER			{ $$ = g_strdup_printf ("%d", (gint)$1); }
	| expanded_string		{ $$ = $1; }
	;

selector_pseudo_identifier
	: T_IDENTIFIER			{ $$ = $1; }
	| T_KEY_FROM			{ $$ = g_strdup ("from"); }
	| T_KEY_TEMPLATES		{ $$ = g_strdup ("templates"); }
	| expanded_string		{ $$ = $1; }
	;

selector_pseudo
	: ':' selector_pseudo_identifier '(' nth ')'
					{ cpg_parser_context_push_selector_pseudo (context, $2, $4); errb }
	| ':' selector_pseudo_identifier		{ cpg_parser_context_push_selector_pseudo (context, $2, NULL); errb }
	;

directional
	:				{ $$ = FALSE; }
	| T_KEY_BIDIRECTIONAL		{ $$ = TRUE; }
	;

all
	:				{ $$ = FALSE; }
	| T_KEY_ALL			{ $$ = TRUE; }
	;

coupling
	: T_KEY_ATTACH all directional selector T_KEY_FROM selector T_KEY_TO selector
					{ cpg_parser_context_link (context, $4, $6, $8, $3, $2); errb }
	;

import
	: T_KEY_IMPORT T_IDENTIFIER T_KEY_FROM expanded_string
					{ cpg_parser_context_import (context, $2, $4); errb }
	;

apply_template
	: T_KEY_APPLY T_KEY_TEMPLATES selector T_KEY_TO selector
					{ cpg_parser_context_apply_template (context, $5, $3); errb }
	;

%%

static void
yyerror (YYLTYPE *locp, CpgParserContext *context, char *s)
{
	cpg_parser_context_error (context,
	                          s);
}

static CpgFunctionPolynomialPiece *
create_polynomial_piece (gdouble  start,
                         gdouble  end,
                         GArray  *coefficients)
{
	guint len;
	gdouble *coefs = NULL;

	len = (guint)(coefficients ? coefficients->len : 0);

	if (coefficients && len > 0)
	{
		guint i;

		coefs = g_new (gdouble, len);

		for (i = 0; i < len; ++i)
		{
			coefs[i] = g_array_index (coefficients, gdouble, i);
		}
	}

	return g_object_ref_sink (cpg_function_polynomial_piece_new (start,
	                                                             end,
	                                                             coefs,
	                                                             len));
}

static CpgFunctionArgument *
create_function_argument (gchar const *name,
                          gboolean     is_optional,
                          gdouble      default_value)
{
	return g_object_ref_sink (cpg_function_argument_new (name,
	                                                     is_optional,
	                                                     default_value));
}
