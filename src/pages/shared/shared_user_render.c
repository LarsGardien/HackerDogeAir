#include "pages/shared/shared_user_render.h"

#include <kore/kore.h>
#include <mustache.h>

uintmax_t
user_varget(mustache_api_t *api, void *userdata, mustache_token_variable_t *token)
{
    UserContext *ctx = (UserContext *) userdata;
    const char *output_string = NULL;

    if(strncmp("id", token->text, token->text_length) == 0)
    {
        if(NULL == ctx->user)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else
        {
            char id_string[12];
            if(snprintf(id_string, 12, "%d", ctx->user->identifier) <= 0)
            {
                kore_log(LOG_ERR, 
                    "user_varget: failed int to string conversion for timeout. input: %d",
                    ctx->user->identifier);
                return (SHARED_RENDER_MUSTACHE_FAIL);
            }
            output_string = id_string;
        }
    }
    else if(strncmp("email", token->text, token->text_length) == 0)
    {
        if(NULL == ctx->user || NULL == ctx->user->email)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else
        {
            output_string = ctx->user->email;
        }
    }
    else if (strncmp("firstname", token->text, token->text_length) == 0)
    {
        if(NULL == ctx->user || NULL == ctx->user->first_name)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else
        {
            output_string = ctx->user->first_name;
        }
    }
    else if (strncmp("lastname", token->text, token->text_length) == 0)
    {
        if(NULL == ctx->user || NULL == ctx->user->last_name)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else
        {
            output_string = ctx->user->last_name;
        }
    }
    else if (strncmp("username", token->text, token->text_length) == 0)
    {
        if(NULL == ctx->user || NULL == ctx->user->username)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else
        {
            output_string = ctx->user->username;
        }
    }    
    else if (strncmp("telnumber", token->text, token->text_length) == 0)
    {
        if(NULL == ctx->user || NULL == ctx->user->telephone_number)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else
        {
            output_string = ctx->user->telephone_number;
        }
    }
    else if(strncmp("error_message", token->text, token->text_length) == 0)
    {
        if(NULL == ctx->error_message)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else
        {
            output_string = ctx->error_message;
        }
    }
    else if(strncmp("role", token->text, token->text_length) == 0)
    {
        if (NULL == ctx->user)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else 
        {
            Role role = ctx->user->role;
            switch(role)
            {
                case USER:
                    output_string = "User";
                    break;
                case ADMIN:
                    output_string = "Admin";
                    break;
                default:
                    output_string = (SHARED_RENDER_EMPTY_STRING);
                    break;

            }
        }
    }
    else if(strncmp("dogecoins", token->text, token->text_length) == 0)
    {
        if (NULL == ctx->user)
        {
            output_string = (SHARED_RENDER_EMPTY_STRING);
        }
        else 
        {
            char dogecoin_string[12];
            if(snprintf(dogecoin_string, 12, "%d", ctx->user->doge_coin) <= 0)
            {
                kore_log(LOG_ERR, 
                    "user_varget: failed int to string conversion for timeout. input: %d",
                    ctx->user->doge_coin);
                return (SHARED_RENDER_MUSTACHE_FAIL);
            }
            output_string = dogecoin_string;
        }
    }

    if(NULL == output_string)
    {
        kore_log(LOG_INFO, "user_varget: unknown template variable '%s'", token->text);
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }

    ctx->partial_context.should_html_escape = true;
    if(api->write(api, userdata, output_string, strlen(output_string)) 
        != (SHARED_RENDER_MUSTACHE_OK))
    {
        kore_log(LOG_ERR, "user_varget: failed to write");
        return (SHARED_RENDER_MUSTACHE_FAIL);
    }
    return (SHARED_RENDER_MUSTACHE_OK);
}