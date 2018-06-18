#include "pages/partial/partial_render.h"

#include <stdio.h>
#include <kore/kore.h>
#include <mustache.h>

#include "shared/shared_error.h"
#include "pages/partial/header/header_render.h"

int shared_render(SharedContext *, mustache_api_t *, const char* const);
int shared_render_mustache_render(mustache_api_t *api, void *context);
void shared_render_clean(SharedContext *context);
void shared_render_copy_context(SharedContext *, SharedContext *);
int shared_render_create_str_context(SharedContext *, const char* const);
uintmax_t shared_varget(mustache_api_t *, void *, mustache_token_variable_t *);
uintmax_t shared_sectget(mustache_api_t *, void *, mustache_token_section_t *);
uintmax_t shared_strread(mustache_api_t *, void *, char *, uintmax_t);
uintmax_t shared_strwrite(mustache_api_t *, void *, char const *, uintmax_t);
uintmax_t shared_mustache_strwrite(mustache_api_t *, void *, char const *, uintmax_t);
void shared_error(mustache_api_t *, void *, uintmax_t, char const *);

const char* const SHARED_RENDER_EMPTY_STRING = "";
const char* const SHARED_RENDER_INVALID_STRING = "invalid";
const char* const SHARED_RENDER_HIDDEN_STRING = "hidden";

int
shared_render(SharedContext *context, mustache_api_t *api, const char* const template_string)
{
    int err = 0; 

    //render all partials
    SharedContext copy_context;
    shared_render_copy_context(context, &copy_context);
    if((err = shared_render_create_str_context(&copy_context, template_string)) 
        != (SHARED_ERROR_OK))
    {
        if(err == SHARED_RENDER_ERROR_ALLOC)
        {   // do not clean up when allocation failed.
            return err;
        }
        goto exit;
    }
    mustache_api_t shared_api={
        .read = &shared_strread,
        .write = &shared_strwrite,
        .varget = &shared_varget,
        .sectget = &shared_sectget,
        .error = &shared_error,
    };
    if((err = shared_render_mustache_render(&shared_api, &copy_context)) != (SHARED_ERROR_OK))
    {
        goto exit;
    }

    //render all page specifics using supplied api
    if((err = shared_render_create_str_context(context,
     (const char* const)copy_context.dst_context->string)) != (SHARED_ERROR_OK))
    {
        goto exit;
    }
    if((err = shared_render_mustache_render(api, context)) != (SHARED_ERROR_OK))
    {
        kore_log(LOG_INFO, "shared_render mustache_render specific");
        goto exit;
    }

    err = (SHARED_ERROR_OK);

exit:
    shared_render_clean(&copy_context);
    return err;
}

int
shared_render_mustache_render(mustache_api_t *api, void *context) 
{
    int err;

    mustache_template_t *template = mustache_compile(api, context);
    if(NULL == template)
    {
        return (SHARED_RENDER_ERROR_TEMPLATE);
    }
    if((err = mustache_render(api, context, template)) == (SHARED_RENDER_MUSTACHE_FAIL))
    {
        return (SHARED_RENDER_ERROR_RENDER);
    }
    mustache_free(api, template);

    return (SHARED_ERROR_OK);
}

void
shared_render_clean(SharedContext *context)
{
    free(context->src_context);
    free(context->dst_context->string);
    free(context->dst_context);
    context->src_context = NULL;
    context->dst_context->string = NULL;
    context->dst_context = NULL;
}

void
shared_render_copy_context(SharedContext *src, SharedContext *dst)
{
    dst->session_id = src->session_id;
    dst->src_context = NULL;
    dst->dst_context = NULL;
}

int 
shared_render_create_str_context(SharedContext *context, const char* const template)
{
    //TODO: benefits of using kore_alloc ?
    context->src_context = (mustache_str_ctx *)malloc(sizeof(mustache_str_ctx));
    context->dst_context = (mustache_str_ctx *)malloc(sizeof(mustache_str_ctx));

    if(NULL == context->src_context || NULL == context->dst_context)
    {
        return (SHARED_RENDER_ERROR_ALLOC);
    }
#pragma GCC diagnostic push  // require GCC 4.6
#pragma GCC diagnostic ignored "-Wcast-qual"
    context->src_context->string = (char *)template;
#pragma GCC diagnostic pop
    context->src_context->offset = 0;
    context->dst_context->string = NULL;
    context->dst_context->offset = 0;

    return (SHARED_ERROR_OK);
}

uintmax_t
shared_varget(mustache_api_t *api, void *userdata, mustache_token_variable_t *token)
{
    uintmax_t ret = 0;
    int err = 0;
    SharedContext *ctx = (SharedContext *) userdata;
    SharedContext new_ctx;
    shared_render_copy_context(ctx, &new_ctx);

    if(strncmp("SHARED_HEADER", token->text, token->text_length) == 0)
    {
        if((err = header_render(&new_ctx)) != (SHARED_ERROR_OK))
        {
            return (SHARED_RENDER_MUSTACHE_FAIL);
        }
        ret = api->write(api, 
            userdata, 
            new_ctx.dst_context->string, 
            strlen(new_ctx.dst_context->string));
        header_render_clean(&new_ctx);
        return ret;
    }

    //the found variable was not a partial view. rewrite the found variable.
    int length = token->text_length + 4 + 1; // +4 => curly braces, +1 => \0
    char *buffer = (char *)malloc(length);
    if(NULL == buffer)
    {
        kore_log(LOG_ERR, "failed malloc for non-partial token.");
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    if((err = snprintf(buffer, length, "{{%s}}", token->text)) != length-1) //-1 => exclude \0
    {
        kore_log(LOG_ERR, 
            "failed snprintf for non-partial token: printed: %d - expected: %d", err, length);
        free(buffer);
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    ret = api->write(api, userdata, buffer, length-1); //-1 => exclude \0
    if(ret != (uintmax_t)length-1)
    {
        free(buffer);
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    free(buffer);
    return (SHARED_RENDER_MUSTACHE_OK);
}

uintmax_t
shared_sectget(mustache_api_t *api, void *userdata, mustache_token_section_t *token)
{   // the partial sectget does nothing but rewriting the section tags and format
    int err = 0;
    uintmax_t ret = 0;
    int length = strlen(token->name) + 4 + 1 + 1; // +4 => curly braces, +1 => '#' OR '/', +1 => \0
    
    char *buffer = (char *)malloc(length);
    if(NULL == buffer)
    {
        kore_log(LOG_ERR, "failed malloc for section.");
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }

    if((err = snprintf(buffer, length, "{{#%s}}", token->name)) != length-1) //-1 => exclude \0
    {
        kore_log(LOG_ERR, 
            "failed snprintf for section: printed: %d - expected: %d", err, length);
        free(buffer);
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    ret = api->write(api, userdata, buffer, length-1); //-1 => exclude \0
    if(ret != (uintmax_t)length-1)
    {
        free(buffer);
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    ret = mustache_render(api, userdata, token->section);
    if(ret != (SHARED_RENDER_MUSTACHE_OK))
    {
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    if((err = snprintf(buffer, length, "{{/%s}}", token->name)) != length-1) //-1 => exclude \0
    {
        kore_log(LOG_ERR, 
            "failed snprintf for section: printed: %d - expected: %d", err, length);
        free(buffer);
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    ret = api->write(api, userdata, buffer, length-1); //-1 => exclude \0
    if(ret != (uintmax_t)length-1)
    {
        free(buffer);
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    return (SHARED_RENDER_MUSTACHE_OK);
}

uintmax_t
shared_strread(mustache_api_t *api, void *userdata, char *buffer, uintmax_t buffer_size)
{
    char *string;
    uintmax_t string_len;
    mustache_str_ctx *ctx = ((SharedContext *)userdata)->src_context; 
    
    string     = ctx->string + ctx->offset;
    string_len = strlen(string);
    if(string_len >= buffer_size)
    {
        string_len = buffer_size;
    }
    
    memcpy(buffer, string, string_len);
    
    ctx->offset += string_len;
    return string_len;
}

uintmax_t
shared_strwrite(mustache_api_t *api, void *userdata, char const *buffer, uintmax_t buffer_size)
{   //TODO: xss prevention by html encoding
    mustache_str_ctx *ctx = ((SharedContext *)userdata)->dst_context; 

    ctx->string = (char *)realloc(ctx->string, ctx->offset + buffer_size + 1);
    
    memcpy(ctx->string + ctx->offset, buffer, buffer_size);
    ctx->string[ctx->offset + buffer_size] = '\0';
    
    ctx->offset += buffer_size;
    return buffer_size;
}

uintmax_t
shared_mustache_strwrite(mustache_api_t *api, void *userdata, char const *buffer, uintmax_t buffer_size)
{
    mustache_str_ctx *ctx = *(mustache_str_ctx **)userdata; 
    
    ctx->string = realloc(ctx->string, ctx->offset + buffer_size + 1);
    
    memcpy(ctx->string + ctx->offset, buffer, buffer_size);
    ctx->string[ctx->offset + buffer_size] = '\0';
    
    ctx->offset += buffer_size;
    return buffer_size;
}

void
shared_error(mustache_api_t *api, void *userdata, uintmax_t lineno, char const *error)
{
    kore_log(LOG_ERR, "mustache error: %d: %s\n", (int)lineno, error);
}