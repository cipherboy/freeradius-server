/*
 * pam.c	Functions to access the PAM library. This was taken
 *		from the hacks that miguel a.l. paraz <map@iphil.net>
 *		did on radiusd-cistron-1.5.3 and migrated to a
 *		separate file.
 *
 *		That, in fact, was again based on the original stuff
 *		from Jeph Blaize <jblaize@kiva.net> done in May 1997.
 *
 * Version:	$Id$
 *
 */

#include	"autoconf.h"

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/time.h>
#include	<netinet/in.h>

#include	<stdio.h>
#include	<string.h>
#include	<pwd.h>
#include	<time.h>
#include	<ctype.h>

#include	<security/pam_appl.h>

#if HAVE_MALLOC_H
#  include	<malloc.h>
#endif

#include	"radiusd.h"
#include	"modules.h"

#ifndef PW_PAM_AUTH
#define PW_PAM_AUTH 1041
#endif

/*************************************************************************
 *
 *	Function: PAM_conv
 *
 *	Purpose: Dialogue between RADIUS and PAM modules.
 *
 * jab - stolen from pop3d
 *************************************************************************/

static char *PAM_username;
static char *PAM_password;
static int PAM_error =0;

static int PAM_conv (int num_msg,
                     const struct pam_message **msg,
                     struct pam_response **resp,
                     void *appdata_ptr) {
  int count = 0, replies = 0;
  struct pam_response *reply = NULL;
  int size = sizeof(struct pam_response);

  appdata_ptr = appdata_ptr;	/* shut the compiler up */
  
#define GET_MEM if (reply) realloc(reply, size); else reply = malloc(size); \
  if (!reply) return PAM_CONV_ERR; \
  size += sizeof(struct pam_response)
#define COPY_STRING(s) (s) ? strdup(s) : NULL
				     
  for (count = 0; count < num_msg; count++) {
    switch (msg[count]->msg_style) {
    case PAM_PROMPT_ECHO_ON:
      GET_MEM;
      reply[replies].resp_retcode = PAM_SUCCESS;
      reply[replies++].resp = COPY_STRING(PAM_username);
      /* PAM frees resp */
      break;
    case PAM_PROMPT_ECHO_OFF:
      GET_MEM;
      reply[replies].resp_retcode = PAM_SUCCESS;
      reply[replies++].resp = COPY_STRING(PAM_password);
      /* PAM frees resp */
      break;
    case PAM_TEXT_INFO:
      /* ignore it... */
      break;
    case PAM_ERROR_MSG:
    default:
      /* Must be an error of some sort... */
      free (reply);
      PAM_error = 1;
      return PAM_CONV_ERR;
    }
  }
  if (reply) *resp = reply;

  return PAM_SUCCESS;
}

struct pam_conv conv = {
  PAM_conv,
  NULL
};

/*************************************************************************
 *
 *	Function: pam_pass
 *
 *	Purpose: Check the users password against the standard UNIX
 *		 password table + PAM.
 *
 * jab start 19970529
 *************************************************************************/

/* cjd 19980706
 * 
 * for most flexibility, passing a pamauth type to this function
 * allows you to have multiple authentication types (i.e. multiple
 * files associated with radius in /etc/pam.d)
 */
static int pam_pass(char *name, char *passwd, const char *pamauth)
{
    pam_handle_t *pamh=NULL;
    int retval;

    PAM_username = name;
    PAM_password = passwd;

    DEBUG("pam_pass: using pamauth string <%s> for pam.conf lookup", pamauth);
    retval = pam_start(pamauth, name, &conv, &pamh);
    if (retval != PAM_SUCCESS) {
      DEBUG("pam_pass: function pam_start FAILED for <%s>. Reason: %s",
	    name, pam_strerror(pamh, retval));
      return -1;
    }

    retval = pam_authenticate(pamh, 0);
    if (retval != PAM_SUCCESS) {
      DEBUG("pam_pass: function pam_authenticate FAILED for <%s>. Reason: %s",
	    name, pam_strerror(pamh, retval));
      pam_end(pamh, 0);
      return -1;
    }

    retval = pam_acct_mgmt(pamh, 0);
    if (retval != PAM_SUCCESS) {
      DEBUG("pam_pass: function pam_acct_mgmt FAILED for <%s>. Reason: %s",
	    name, pam_strerror(pamh, retval));
      pam_end(pamh, 0);
      return -1;
    }

    DEBUG("pam_pass: authentication succeeded for <%s>", name);
    pam_end(pamh, 0);
    return 0;
}

/* translate between function declarations */
static int pam_auth(REQUEST *request, char *username, char *password)
{
	int	r;
	VALUE_PAIR *pair;
	const char *pam_auth_string = "radiusd";

	pair = pairfind(request->config_items, PW_PAM_AUTH);
	if (pair) pam_auth_string = pair->strvalue;

	r = pam_pass(username, password, pam_auth_string);
	return (r == 0) ? RLM_AUTH_OK : RLM_AUTH_REJECT;
}

module_t rlm_pam = {
  "Pam",
  0,				/* type: reserved */
  NULL,				/* initialize */
  NULL,				/* authorize */
  pam_auth,			/* authenticate */
  NULL,				/* accounting */
  NULL,				/* detach */
};

