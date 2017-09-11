/*
 * This file is part of cparser.
 * Copyright (C) 2012 Matthias Braun <matze@braunis.de>
 */
#ifndef WRITE_NIM_H
#define WRITE_NIM_H

#include "ast/ast.h"

void write_nim(FILE *out, const translation_unit_t *unit);

#endif
