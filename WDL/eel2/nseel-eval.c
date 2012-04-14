/*
  Expression Evaluator Library (NS-EEL) v2
  Copyright (C) 2004-2008 Cockos Incorporated
  Copyright (C) 1999-2003 Nullsoft, Inc.
  
  nseel-eval.c

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <string.h>
#include <ctype.h>
#include "ns-eel-int.h"

#define NSEEL_VARS_MALLOC_CHUNKSIZE 8
#define NSEEL_GLOBALVAR_BASE (1<<24)

#define strnicmp(x,y,z) strncasecmp(x,y,z)


#define INTCONST 1
#define DBLCONST 2
#define HEXCONST 3
#define VARIABLE 4
#define OTHER    5

EEL_F nseel_globalregs[100];

//------------------------------------------------------------------------------
void *nseel_compileExpression(compileContext *ctx, char *exp)
{
  ctx->errVar=0;
  nseel_llinit(ctx);
  if (!nseel_yyparse(ctx,exp) && !ctx->errVar)
  {
    return (void*)ctx->result;
  }
  return 0;
}

//------------------------------------------------------------------------------

void NSEEL_VM_enumallvars(NSEEL_VMCTX ctx, int (*func)(const char *name, EEL_F *val, void *ctx), void *userctx)
{
  compileContext *tctx = (compileContext *) ctx;
  int wb;
  if (!tctx) return;

  for (wb = 0; wb < tctx->varTable_numBlocks; wb ++)
  {
    int ti;
    int namepos=0;
    for (ti = 0; ti < NSEEL_VARS_PER_BLOCK; ti ++)
    {        
      char *p=tctx->varTable_Names[wb]+namepos;
      if (!*p) break;


      if (!func(p,&tctx->varTable_Values[wb][ti],userctx)) 
        break;

      namepos += NSEEL_MAX_VARIABLE_NAMELEN;
    }
    if (ti < NSEEL_VARS_PER_BLOCK)
      break;
  }
}



INT_PTR nseel_int_register_var(compileContext *ctx, const char *name, EEL_F **ptr)
{
  int wb;
  int ti=0;
  int i=0;
  char *nameptr;

  if (ctx->function_localTable_Size &&
      ctx->function_localTable_Names &&
      ctx->function_localTable_Values)
  {
    int n=ctx->function_localTable_Size;
    char *p =ctx->function_localTable_Names;
    while (n-->0)
    {
      if (!strnicmp(name,p,NSEEL_MAX_VARIABLE_NAMELEN))
      {
        if (ptr) *ptr = ctx->function_localTable_Values + i;
        return i;
      }
      p += NSEEL_MAX_VARIABLE_NAMELEN;
      i++;
    }
  }

  for (wb = 0; wb < ctx->varTable_numBlocks; wb ++)
  {
    int namepos=0;
    for (ti = 0; ti < NSEEL_VARS_PER_BLOCK; ti ++)
    {        
      if (!ctx->varTable_Names[wb][namepos] || !strnicmp(ctx->varTable_Names[wb]+namepos,name,NSEEL_MAX_VARIABLE_NAMELEN))
      {
        break;
      }
      namepos += NSEEL_MAX_VARIABLE_NAMELEN;
      i++;
    }
    if (ti < NSEEL_VARS_PER_BLOCK)
      break;
  }
  if (wb == ctx->varTable_numBlocks)
  {
    ti=0;
    // add new block
    if (!(ctx->varTable_numBlocks&(NSEEL_VARS_MALLOC_CHUNKSIZE-1)) || !ctx->varTable_Values || !ctx->varTable_Names )
    {
      ctx->varTable_Values = (EEL_F **)realloc(ctx->varTable_Values,(ctx->varTable_numBlocks+NSEEL_VARS_MALLOC_CHUNKSIZE) * sizeof(EEL_F *));
      ctx->varTable_Names = (char **)realloc(ctx->varTable_Names,(ctx->varTable_numBlocks+NSEEL_VARS_MALLOC_CHUNKSIZE) * sizeof(char *));
    }
    ctx->varTable_numBlocks++;

    ctx->varTable_Values[wb] = (EEL_F *)calloc(sizeof(EEL_F),NSEEL_VARS_PER_BLOCK);
    ctx->varTable_Names[wb] = (char *)calloc(NSEEL_MAX_VARIABLE_NAMELEN,NSEEL_VARS_PER_BLOCK);
  }

  nameptr=ctx->varTable_Names[wb]+ti*NSEEL_MAX_VARIABLE_NAMELEN;
  if (!nameptr[0])
  {
    strncpy(nameptr,name,NSEEL_MAX_VARIABLE_NAMELEN);
  }
  if (ptr) *ptr = ctx->varTable_Values[wb] + ti;
  return i;
}


//------------------------------------------------------------------------------
EEL_F *NSEEL_VM_regvar(NSEEL_VMCTX _ctx, const char *var)
{
  compileContext *ctx = (compileContext *)_ctx;
  EEL_F *r;
  if (!ctx) return 0;

  if (!strnicmp(var,"reg",3) && strlen(var) == 5 && isdigit(var[3]) && isdigit(var[4]))
  {
    int x=atoi(var+3);
    if (x < 0 || x > 99) x=0;
    return nseel_globalregs + x;
  }

  nseel_int_register_var(ctx,var,&r);

  return r;
}

//------------------------------------------------------------------------------
INT_PTR nseel_translate(compileContext *ctx, int type)
{
  int v;
  int n;
  char tmp[256];
  nseel_gettoken(ctx,tmp, sizeof(tmp));

  switch (type)
  {
    case INTCONST: return nseel_createCompiledValue(ctx,(EEL_F)atoi(tmp)); // todo: this could be atof() eventually
    case DBLCONST: return nseel_createCompiledValue(ctx,(EEL_F)atof(tmp));
    case HEXCONST:
      v=0;
      n=0;
      while (1)
      {
        int a=tmp[n++];
        if (a >= '0' && a <= '9') v=(v<<4)+a-'0';
        else if (a >= 'A' && a <= 'F') v=(v<<4)+10+a-'A';
        else if (a >= 'a' && a <= 'f') v=(v<<4)+10+a-'a';
        else break;
      }
		return nseel_createCompiledValue(ctx,(EEL_F)v);
  }
  return 0;
}

//------------------------------------------------------------------------------
INT_PTR nseel_lookup(compileContext *ctx, int *typeOfObject)
{
  char tmp[256];
  int i;

  nseel_gettoken(ctx,tmp, sizeof(tmp));

  if (!strnicmp(tmp,"reg",3) && strlen(tmp) == 5 && isdigit(tmp[3]) && isdigit(tmp[4]) && (i=atoi(tmp+3))>=0 && i<100)
  {
    *typeOfObject=IDENTIFIER;
    return nseel_createCompiledValuePtr(ctx,nseel_globalregs+i-NSEEL_GLOBALVAR_BASE);
  }
  

  {
    const char *nptr = tmp;

#ifdef NSEEL_EEL1_COMPAT_MODE
    if (!strcasecmp(nptr,"if")) nptr="_if";
    else if (!strcasecmp(nptr,"bnot")) nptr="_not";
    else if (!strcasecmp(nptr,"assign")) nptr="_set";
    else if (!strcasecmp(nptr,"equal")) nptr="_equal";
    else if (!strcasecmp(nptr,"below")) nptr="_below";
    else if (!strcasecmp(nptr,"above")) nptr="_above";
    else if (!strcasecmp(nptr,"megabuf")) nptr="_mem";
    else if (!strcasecmp(nptr,"gmegabuf")) nptr="_gmem";
#endif

    for (i=0;nseel_getFunctionFromTable(i);i++)
    {
      functionType *f=nseel_getFunctionFromTable(i);
      if (!strcasecmp(f->name, nptr))
      {
        switch (f->nParams&0xff)
        {
          case 0:
          case 1: *typeOfObject = FUNCTION1; break;
          case 2: *typeOfObject = FUNCTION2; break;
          case 3: *typeOfObject = FUNCTION3; break;
          default: 
            *typeOfObject = FUNCTION1;  // should never happen, unless the caller was silly
          break;
        }
        return nseel_createCompiledFunctionCall(ctx,f->nParams&0xff,FUNCTYPE_FUNCTIONTYPEREC,(INT_PTR) f);
      }
    }
  }
  
  // scan for parameters/local variables before user functions   
  if (ctx->function_localTable_Names && ctx->function_localTable_Values)
  {
    const char *p = ctx->function_localTable_Names;
    for (i=0; i < ctx->function_localTable_Size; i++)
    {
      if (!strnicmp(p,tmp,NSEEL_MAX_VARIABLE_NAMELEN))
      {
        *typeOfObject = IDENTIFIER;
        return nseel_createCompiledValuePtr(ctx, ctx->function_localTable_Values + i);
      }
      p += NSEEL_MAX_VARIABLE_NAMELEN;
    }
  }


  {
    _codeHandleFunctionRec *fr = ctx->functions_local;
    _codeHandleFunctionRec *bmatch=NULL;
    int is_this_ptr = 0;    
    char *postName = tmp;

    while (*postName) postName++;
    while (postName >= tmp && *postName != '.') postName--;

    if (++postName <= tmp) postName=0;
    else is_this_ptr = !strnicmp(tmp,"this.",5) && tmp[5];

    while (fr)
    {
      if (!is_this_ptr && !strcasecmp(fr->fname,tmp))
      {
        *typeOfObject=fr->num_params>=3?FUNCTION3 : fr->num_params==2?FUNCTION2 : FUNCTION1;
        return nseel_createCompiledFunctionCall(ctx,fr->num_params,FUNCTYPE_EELFUNC,(INT_PTR)fr);
      }

      if (!bmatch && postName && !strcasecmp(fr->fname,postName)) 
      {
        bmatch=fr;
        if (is_this_ptr) break; // if not searching for actual function, bmatch is good enough!
      }

      fr=fr->next;
    }

    if (!is_this_ptr || !bmatch)
    {
      fr = ctx->functions_common;
      while (fr)
      {
        if (!is_this_ptr && !strcasecmp(fr->fname,tmp))
        {
          *typeOfObject=fr->num_params>=3?FUNCTION3 : fr->num_params==2?FUNCTION2 : FUNCTION1;
          return nseel_createCompiledFunctionCall(ctx,fr->num_params,FUNCTYPE_EELFUNC,(INT_PTR)fr);
        }
        if (!bmatch && postName && !strcasecmp(fr->fname,postName)) 
        {
          bmatch=fr;
          if (is_this_ptr) break;
        }
        fr=fr->next;
      }
    }

    if (bmatch) 
    {     
      *typeOfObject=bmatch->num_params>=3?FUNCTION3 : bmatch->num_params==2?FUNCTION2 : FUNCTION1;
      if (!bmatch->usesThisPointer) 
      {
        // if bmatch doesn't access this, no need to implement a unique function, just call the main function
        return nseel_createCompiledFunctionCall(ctx,bmatch->num_params,FUNCTYPE_EELFUNC,(INT_PTR)bmatch);
      }

      if (is_this_ptr)
      {
        ctx->function_usesThisPointer = 1;
        return nseel_createCompiledFunctionCallEELThis(ctx,bmatch->num_params,(INT_PTR)bmatch, tmp+5);
      }
  
      // we can go ahead and create our fully qualified function instance
      fr = eel_createFunctionNamespacedInstance(ctx,bmatch,tmp);
      if (!fr) fr=bmatch;
      return nseel_createCompiledFunctionCall(ctx,fr->num_params,FUNCTYPE_EELFUNC,(INT_PTR)fr);
    }
  }

  if (!strnicmp(tmp,"this.",5) && tmp[5])
  {
    ctx->function_usesThisPointer=1;
    *typeOfObject = IDENTIFIER;
    return nseel_createCompiledValueFromNamespaceName(ctx,tmp+5); 
    // 
  }

  {
    int wb;
    for (wb = 0; wb < ctx->varTable_numBlocks; wb ++)
    {
      int namepos=0;
      int ti;
      for (ti = 0; ti < NSEEL_VARS_PER_BLOCK; ti ++)
      {        
        if (!ctx->varTable_Names[wb][namepos]) break;

        if (!strnicmp(ctx->varTable_Names[wb]+namepos,tmp,NSEEL_MAX_VARIABLE_NAMELEN))
        {
          *typeOfObject = IDENTIFIER;
          return nseel_createCompiledValuePtr(ctx,ctx->varTable_Values[wb] + ti); 
        }

        namepos += NSEEL_MAX_VARIABLE_NAMELEN;
      }
      if (ti < NSEEL_VARS_PER_BLOCK) break;
    }
  }

  *typeOfObject = IDENTIFIER;
  {
    EEL_F *p=NULL;
    nseel_int_register_var(ctx,tmp,&p);
    if (p) return nseel_createCompiledValuePtr(ctx,p); 
  }
  return nseel_createCompiledValue(ctx,0.0);
}


//---------------------------------------------------------------------------
int nseel_yyerror(compileContext *ctx)
{
  ctx->errVar = ctx->colCount;
  return 0;
}
