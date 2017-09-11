#include "adt/panic.h"
#include "adt/separator_t.h"
#include "ast/ast_t.h"
#include "ast/entity_t.h"
#include "ast/printer.h"
#include "ast/symbol_t.h"
#include "ast/type.h"
#include "ast/type_t.h"
#include "write_nim.h"

static const scope_t *global_scope;
static FILE          *out;

static void write_type(const type_t *type);


static const char *get_atomic_type_string(const atomic_type_kind_t type, bool is_pointer)
{
	switch(type) {
    case ATOMIC_TYPE_CHAR:        return is_pointer ? "cstring #[ cchar* ]#" : "cchar";
	  case ATOMIC_TYPE_SCHAR:       return is_pointer ? "cstring #[ cschar* ]#" : "cschar";
	  case ATOMIC_TYPE_UCHAR:       return is_pointer ? "cstring #[ cuchar* ]#" : "cuchar";
	  case ATOMIC_TYPE_SHORT:       return "cshort";
	  case ATOMIC_TYPE_USHORT:      return "cushort";
	  case ATOMIC_TYPE_INT:         return "cint";
	  case ATOMIC_TYPE_UINT:        return "cuint";
	  case ATOMIC_TYPE_LONG:        return "clong";
	  case ATOMIC_TYPE_ULONG:       return "culong";
	  case ATOMIC_TYPE_LONGLONG:    return "clonglong";
	  case ATOMIC_TYPE_ULONGLONG:   return "culonglong";
	  case ATOMIC_TYPE_FLOAT:       return "cfloat";
	  case ATOMIC_TYPE_DOUBLE:      return "cdouble";
	  case ATOMIC_TYPE_LONG_DOUBLE: return "clongdouble";
	  case ATOMIC_TYPE_BOOL:        return "bool";
	  default:                      panic("unsupported atomic type");
	}
}

static void write_atomic_type(const atomic_type_t *type, bool is_pointer)
{
	fprintf(out, "%s", get_atomic_type_string(type->akind, is_pointer));
}

static void write_pointer_type(const pointer_type_t *ptype)
{
  const type_t *type = ptype->points_to;
	switch(type->kind) {
	case TYPE_ATOMIC:
		write_atomic_type(&type->atomic, true);
		break;
	default:
	  fprintf(out, "ref ");
		write_type(type);
		break;
	}
}

static entity_t const *find_typedef(const type_t *type)
{
	/* first: search for a matching typedef in the global type... */
	for (entity_t const *entity = global_scope->first_entity; entity != NULL;
	     entity = entity->base.next) {
		if (entity->kind != ENTITY_TYPEDEF)
			continue;
		if (entity->declaration.type == type)
			return entity;
	}
	return NULL;
}


static void write_compound_type(const compound_type_t *type)
{
	entity_t const *entity = find_typedef((const type_t*) type);
	if (entity != NULL) {
		fprintf(out, "%s", entity->base.symbol->string);
		return;
	}

	/* does the struct have a name? */
	symbol_t *symbol = type->compound->base.symbol;
	if (symbol != NULL) {
		/* TODO: make sure we create a struct for it... */
		fprintf(out, "%s", symbol->string);
		return;
	}
	/* TODO: create a struct and use its name here... */
	fprintf(out, "object");
}

static void write_typedef_type(const typedef_type_t *type)
{
	entity_t const *entity = (entity_t*)type->typedefe;
	if (entity != NULL) {
		fprintf(out, "%s", entity->base.symbol->string);
		return;
	}

	fprintf(out, "object");
}

static void write_enum_type(const enum_type_t *type)
{
	entity_t const *entity = find_typedef((const type_t*) type);
	if (entity != NULL) {
		fprintf(out, "%s", entity->base.symbol->string);
		return;
	}

	/* does the enum have a name? */
	symbol_t *symbol = type->enume->base.symbol;
	if (symbol != NULL) {
		/* TODO: make sure we create an enum for it... */
		fprintf(out, "%s", symbol->string);
		return;
	}
	/* TODO: create a struct and use its name here... */
	fprintf(out, "object");
}

static void write_function_type(const function_type_t *type)
{
	fprintf(out, "proc (");

	function_parameter_t *parameter = type->parameters;
	separator_t           sep       = { "", ", " };
  int i = 0;
	while(parameter != NULL) {
		fputs(sep_next(&sep), out);

    write_type(parameter->type);
    fprintf(out, "_%d: ", i);
		write_type(parameter->type);

		parameter = parameter->next;
    i++;
	}


	fprintf(out, "): ");
	write_type(type->return_type);
	fprintf(out, ")");
}


static void write_type(const type_t *type)
{
	switch(type->kind) {
	case TYPE_ATOMIC:
		write_atomic_type(&type->atomic, false);
		return;
	case TYPE_POINTER:
		write_pointer_type(&type->pointer);
		return;
	case TYPE_COMPOUND_UNION:
	case TYPE_COMPOUND_STRUCT:
		write_compound_type(&type->compound);
		return;
  case TYPE_TYPEDEF:
    write_typedef_type(&type->typedeft);
    return;
	case TYPE_ENUM:
		write_enum_type(&type->enumt);
		return;
	case TYPE_FUNCTION:
		write_function_type(&type->function);
		return;
  //case TYPE_ARRAY:
  //  write_array_type(&type->array);
	case TYPE_VOID:
		fputs("pointer", out);
		return;
	case TYPE_COMPLEX:
	case TYPE_IMAGINARY:
	default:
		fprintf(out, "pointer");
		break;
	}
}

static void write_compound_entry(const entity_t *entity)
{
	fprintf(out, "  %s: ", entity->base.symbol->string);
	write_type(entity->declaration.type);
	fprintf(out, "\n");
}

static void write_compound(const symbol_t *symbol, const compound_type_t *type)
{
	fprintf(out, "type %s%s = object\n",
			    symbol->string,
	        type->base.kind == TYPE_COMPOUND_UNION ? " {.union.}" : "");

	for (entity_t const *entity = type->compound->members.first_entity;
	     entity != NULL; entity = entity->base.next) {
		write_compound_entry(entity);
	}

	fprintf(out, "\n");
}

static void write_expression(const expression_t *expression);

static void write_unary_expression(const unary_expression_t *expression)
{
	switch(expression->base.kind) {
	case EXPR_UNARY_NEGATE:
		fputc('-', out);
		break;
	case EXPR_UNARY_NOT:
		fputs("not ", out);
		break;
	default:
		panic("unimplemented unary expression");
	}
	write_expression(expression->value);
}

static void write_expression(const expression_t *expression)
{
	switch(expression->kind) {
	case EXPR_LITERAL_INTEGER:
		fprintf(out, "%s", expression->literal.value->begin);
		break;
	case EXPR_UNARY_CASES:
		write_unary_expression((const unary_expression_t*) expression);
		break;
	default:
		panic("not implemented expression");
	}
}

static void write_enum(const symbol_t *symbol, const enum_type_t *type)
{
	fprintf(out, "type %s = enum\n", symbol->string);

	for (const entity_t *entry = type->enume->first_value;
	     entry != NULL && entry->kind == ENTITY_ENUM_VALUE;
	     entry = entry->base.next) {
		fprintf(out, "  %s", entry->base.symbol->string);
		if (entry->enum_value.value != NULL) {
			fprintf(out, " = ");
			write_expression(entry->enum_value.value);
		}
    if(entry->base.next != NULL){
		  fputs(",\n", out);
    }
	}
	fprintf(out, "\n");
}

static void write_variable(const entity_t *entity)
{
	fprintf(out, "var %s: ", entity->base.symbol->string);
	write_type(entity->declaration.type);
	fprintf(out, "\n");
}

static void write_typedef(const entity_t *entity)
{
	fprintf(out, "type %s = distinct ", entity->base.symbol->string);
	write_type(entity->declaration.type);
	fprintf(out, "\n");
}



static void write_function(const entity_t *entity)
{
	if (entity->function.body != NULL) {
		fprintf(stderr, "Warning: can't convert function bodies (at %s)\n",
		        entity->base.symbol->string);
	}

	fprintf(out, "proc %s*(", entity->base.symbol->string);

	const function_type_t *function_type
		= (const function_type_t*) entity->declaration.type;

	separator_t sep       = { "", ", " };
  int i = 0;
	for(entity_t const *parameter = entity->function.parameters.first_entity;
	    parameter != NULL; parameter = parameter->base.next) {
		assert(parameter->kind == ENTITY_PARAMETER);
		fputs(sep_next(&sep), out);
		if (parameter->base.symbol != NULL) {
			fprintf(out, "%s: ", parameter->base.symbol->string);
		} else {
		  write_type(parameter->declaration.type);
			fprintf(out, "_%d: ", i);
		}
		write_type(parameter->declaration.type);
    i++;
	}

	fprintf(out, ")");

	const type_t *return_type = skip_typeref(function_type->return_type);
	if (!is_type_void(return_type)) {
		fprintf(out, ": ");
		write_type(return_type);
	}

  fprintf(out, " {.importc: \"%s\"", entity->base.symbol->string);
	if (function_type->variadic) {
		fputs(sep_next(&sep), out);
		fprintf(out, ", varargs");
	}

  fprintf(out, ".}");

	fputc('\n', out);
}

void write_nim(FILE *output, const translation_unit_t *unit)
{
	out            = output;
	global_scope = &unit->scope;

	print_to_file(out);
	fprintf(out, "# WARNING: Automatically generated file\n");

	/* write functions */
	for(entity_t const *entity = unit->scope.first_entity; entity != NULL;
	    entity = entity->base.next) {
		if (entity->kind != ENTITY_TYPEDEF)
			continue;

		write_typedef(entity);
	}

	/* write structs,unions + enums */
	for(entity_t const *entity = unit->scope.first_entity; entity != NULL;
	    entity = entity->base.next) {
		if (entity->kind != ENTITY_TYPEDEF)
			continue;

		type_t *type = entity->declaration.type;
		if (is_type_compound(type)) {
			write_compound(entity->base.symbol, &type->compound);
		} else if (type->kind == TYPE_ENUM) {
			write_enum(entity->base.symbol, &type->enumt);
		}
	}

	/* write global variables */
	for(entity_t const *entity = unit->scope.first_entity; entity != NULL;
	    entity = entity->base.next) {
		if (entity->kind != ENTITY_VARIABLE)
			continue;

		write_variable(entity);
	}

	/* write functions */
	for(entity_t const *entity = unit->scope.first_entity; entity != NULL;
	    entity = entity->base.next) {
		if (entity->kind != ENTITY_FUNCTION)
			continue;

		write_function(entity);
	}
}
