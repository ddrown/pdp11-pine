#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: addrutil.c,v 4.7 1993/10/18 23:34:58 mikes Exp $";
#endif
/*----------------------------------------------------------------------

            T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade and Mike Seibel
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   Administration Builiding, AG-44
   Seattle, Washington, 98195, USA
   Internet: lgl@CAC.Washington.EDU
             mikes@CAC.Washington.EDU

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"

   Copyright 1989-1993  University of Washington

    Permission to use, copy, modify, and distribute this software and its
   documentation for any purpose and without fee to the University of
   Washington is hereby granted, provided that the above copyright notice
   appears in all copies and that both the above copyright notice and this
   permission notice appear in supporting documentation, and that the name
   of the University of Washington not be used in advertising or publicity
   pertaining to distribution of the software without specific, written
   prior permission.  This software is made available "as is", and
   THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
   WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
   NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
   INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
   (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
   WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
  
   Pine and Pico are trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior
   written permission of the University of Washington.

   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
      addrutil.c

   Miscellaneous routines to manage e-mail addresses

 ====*/

#include "headers.h"


ADDRESS *expand_address(char *, char *, char *);


/*----------------------------------------------------------------------
        Given an address, expand it based on address book, local domian, etc

    Args: to      -- The given address to expand
          to_full -- A buffer for the full address to be returned in.
                     This buffer should be large (MAX_ADDR_FIELD)
          error   -- Pointer to address to put error message if any
 
   Result:  0 is returned if address was OK, 
           -1 if address wasn't OK.
            The address is expanded, fully qualified and personal name added.

Input may have more than one address. If so they must be separated by commas.
Adhereing to the more official "name <addr>" format necesitates this because
the name may be several words. The more common "addr (name)" format would not 
require commas.
  ----*/

int
build_address(to , full_to, error)
     char *to, *full_to, **error;
{
    ADDRESS *a;

    ps_global->c_client_error[0] = '\0';

    a = expand_address(to, ps_global->maildomain,
                         !ps_global->do_local_lookup ? NULL :
                        ps_global->use_full_hostname ? ps_global->hostname :
                           ps_global->localdomain);

    if(ps_global->c_client_error[0] != '\0') {
        /*-- Parse error occured. Return string as is and error message --*/
        strcpy(full_to, to);
        if(error != NULL)
          *error = ps_global->c_client_error;
        dprint(2, (debugfile, "build_address returning parse error: %s\n",
                   ps_global->c_client_error));
        return(-1);
    }

    full_to[0] = '\0';

    rfc822_write_address(full_to, a);

    mail_free_address(&a);

    return(0);
}

      
#ifdef X_NEVER      /* Routine not needed now, but perhaps in the future */
/*----------------------------------------------------------------------
     Get the next address out of a comma separated list

  Input: string which is list, buffer to put next address in

 Result: The next address is copied into the given buffer
         A pointer into the input string where next word starts is returned
  ----------------------------------------------------------------------*/  
char *            
next_address(input, output)
     register char *input, *output;
{
    while(*input == ' ' && *input != '\0')
      input++;

    while(*input != ',' && *input != '\0')
      *output++ = *input++;

    *output = '\0';

    if(*input == ',')
      input++;
    return(input);
}
#endif



/*----------------------------------------------------------------------
      Turn a list of address structures into a formatted string

   Args: addr   -- A linked list of ADDRESS structures

 Result: malloced, comma seperated list of addresses
  ----*/

char *
addr_list_string(addr, wrap)
     ADDRESS *addr;
     int      wrap;
{
    int               len;
    char             *string, *s;
    register ADDRESS *a;
    int               wraplen = 0;

    if(!addr)
      return(cpystr(""));
    
    /* First figure out size */
    len = 0;
    for(a = addr; a != NULL; a = a->next)
      len += strlen(addr_string(a)) + ((wrap) ? 8 : 2);

    string = fs_get(len);
    s      = string;
    s[0]   = '\0';

    for(a = addr; a ; a = a->next) {
	strcpy(s, addr_string(a));
	len = strlen(s);
	if(wrap)		/* ugly, but it gets the job done */
	  wraplen += len;

	s += len;
	if(a->next){
	    *s++ = ',';
	    if(wrap && (wraplen > 80)){		/* may also have to wrap? */
		wraplen = 0;
		*s++    = '\n';
		*s++    = ' ';
		*s++    = ' ';
		*s++    = ' ';
	    }

	    *s++ = ' ';
	}
    }

    return(string);
}


/*----------------------------------------------------------------------
      Format an address structure into a string

   Args: addr -- Singe ADDRESS structure to turn into a string

 Result: returns pointer to internal static formated string
  ----------------------------------------------------------------------*/
char *
addr_string(addr)
     ADDRESS *addr;
{
    ADDRESS  tmp_addr;
    static char  *string = NULL;
    static size_t string_size = 0;
    size_t        need;

    /*
     * rfc822_write_address()/rfc822_cat() do unbounded strcat()s on
     * the assumption the caller's buffer is "big enough" -- true of
     * MAX_ADDR_EXPN (1000 bytes) for an ordinary address, but
     * addr->mailbox/host/personal/adl are cpystr()'d with no length
     * cap anywhere upstream (e.g. parsing a resumed postponed
     * message's To: line), so a long address overflows a fixed
     * buffer here. Confirmed under AddressSanitizer: resuming a
     * postponed message with a 20000-byte To: address overflows this
     * exact buffer via rfc822_cat(), reachable before the composer
     * even redraws. Size dynamically instead, same technique as
     * tmp_20k_buf.
     */
    need = 2 * ((addr->personal ? strlen(addr->personal) : 0)
                + (addr->mailbox ? strlen(addr->mailbox) : 0)
                + (addr->host   ? strlen(addr->host)   : 0)
                + (addr->adl    ? strlen(addr->adl)    : 0))
           + 32;    /* "<>", "@", quotes, group ": ", ";" */
    if(need < MAX_ADDR_EXPN+1)
      need = MAX_ADDR_EXPN+1;

    if(need > string_size){
	if(string)
	  fs_give((void **)&string);
	string = (char *)fs_get(need);
	string_size = need;
    }

    string[0] = '\0';
    tmp_addr = *addr;
    tmp_addr.next = NULL;
    rfc822_write_address(string, &tmp_addr);
    return(string);
}



/*----------------------------------------------------------------------
  Check to see if address is that of the current user running pine

  Args: a  -- Address to check
        ps -- The pine_state structure

 Result: returns 1 if it matches, 0 otherwise.

    Criterion is that the mailbox must match the user name and
(the hostname must match or the full name must match).  In matching the
hostname, matches occur if the hostname on the message is blank, or if it
matches the local hostname, the full hostname with qualifying domain,
or the qualifying domain without a specific host.  Note, there is a very
small chance that we will err on the non-conservative side here.  That is,
we may decide two addresses are the same even though they are different
(because we do case-insensitive compares on the mailbox).  That might cause
a reply not to be sent to somebody because they look like they are us.
This should be very, very rare.
 ----*/
int
address_is_us(a, ps)
     ADDRESS *a;
     struct pine *ps;
{
    if(a == NULL || a->mailbox == NULL)
      return(0);

    /* at least LHS must match */
    if(strucmp(a->mailbox, ps->VAR_USER_ID)==0

                      &&
       /* and either personal name matches or hostname matches */

    /* personal name matches if it's not there or if it actually matches */
   ((a->personal == NULL || strucmp(a->personal , ps->VAR_PERSONAL_NAME) == 0)
                              ||
    /* hostname matches if it's not there, */
    (a->host==NULL ||
       /* or if hostname and userdomain (the one user sets) match exactly, */
      ((ps->userdomain && a->host && strucmp(a->host,ps->userdomain)==0) ||

              /*
               * or if(userdomain is either not set or it is set to be
               * the same as the localdomain or hostname) and (the hostname
               * of the address matches either localdomain or hostname)
               */
             ((ps->userdomain == NULL ||
               strucmp(ps->userdomain, ps->localdomain) == 0 ||
               strucmp(ps->userdomain, ps->hostname) == 0) &&
              (strucmp(a->host, ps->hostname) == 0 ||
               strucmp(a->host, ps->localdomain) == 0))))))
        return(1);

    return(0);
}


/*----------------------------------------------------------------------
     Expand an address string against local names and domain

   Input:  The address string to parse and expand
           The domain the user considers himself to be in
           The domain of the local password file (usually as above)

   Result: A list of fully qualified addresses is returned

Use mtp_parse_address to turn the string into data structures on which
the expansion is then done. This assures all quoting and other formatting
proceeds according to the standard.

If the localdomain is NULL then no lookup against the password file will
be done.
  ----------------------------------------------------------------------*/
ADDRESS *
expand_address(a_string, userdomain, localdomain)
     char   *userdomain, *localdomain, *a_string;
{
    int          domain_length;
    ADDRESS     *adr, *a;
    char        *fakedomain;

    /*-----
        create a fake domain name '@@@@', the same length as the real domain
      name. Then in the common case where the local domain name is filled in
      the right storage is already allocated. We also use the @@@ to detect
      an unqualified address
      -----*/
    domain_length = max(localdomain != NULL ? strlen(localdomain) : 0,
                         userdomain != NULL ? strlen(userdomain)  : 0);
    fakedomain = fs_get(domain_length + 1);
    memset(fakedomain, '@', domain_length);
    fakedomain[domain_length] = '\0';

    adr = NULL;
    rfc822_parse_adrlist(&adr, a_string, fakedomain);

    for(a = adr; a != NULL; a = a->next) {
        if(a->host && a->host[0] == '@'){
            /*-----------
                 Hostname is  '@@@@...' indicating name wasn't qualified.
                 Look up in the password file for the local full name.
                 If no match then fill in the local domain for host
              -----------*/
            if(localdomain != NULL && a->personal == NULL) {
                /*---- lookup in passwd file for local full name ---*/
                a->personal = cpystr(local_name_lookup(a->mailbox));

                strcpy(a->host, a->personal == NULL ? userdomain :
                                                      localdomain);
            } else {
                strcpy(a->host, userdomain);
            }
        }
    }

    fs_give((void **)&fakedomain);

    return(adr);
}


