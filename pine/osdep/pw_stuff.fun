/*----------------------------------------------------------------------
  This collection of routines looks up the login name and password on the
system. For things like PC's it's OK for these to return NULL since there
is no system login. Other code will figure out who the user actually is.
  ----*/
static struct passwd *unix_pwd = NULL;

/*----------------------------------------------------------------------
      Pull the name out of the gcos field if we have that sort of /etc/passwd

   Args: gcos_field --  The long name or GCOS field to be parsed
         logname    --  Replaces occurances of & with logname string

 Result: returns pointer to buffer with name
  ----*/
static char *
gcos_name(gcos_field, logname)
    char *logname, *gcos_field;
{
    char *firstcp, *lastcp;

    /* The last character of the full name is the one preceding the first
     * '('. If there is no '(', then the full name ends at the end of the
     * gcos field.
     */
    if(lastcp = strindex(gcos_field, '('))
	*lastcp = '\0';

    /* The first character of the full name is the one following the 
     * last '-' before that ending character. NOTE: that's why we
     * establish the ending character first!
     * If there is no '-' before the ending character, then the fullname
     * begins at the beginning of the gcos field.
     */
    if(firstcp = strrindex(gcos_field, '-'))
	firstcp++;
    else
	firstcp = gcos_field;

    return(firstcp);
}


char *
get_system_login()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid());
        if(unix_pwd == NULL)
          return(NULL);
    }
    return(unix_pwd->pw_name);
}
char *
get_system_fullname()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid());
        if(unix_pwd == NULL)
          return(NULL);
    }
    return(gcos_name(unix_pwd->pw_gecos, unix_pwd->pw_name));
}
char *
get_system_homedir()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid());
        if(unix_pwd == NULL)
          return(NULL);
    }
    return(unix_pwd->pw_dir);
}
char *
get_system_passwd()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid()); /* Always do real lookup */
        if(unix_pwd == NULL)
          return(NULL);
    }
    return(unix_pwd->pw_passwd);
}


/*----------------------------------------------------------------------
      Look up a userid on the local system and return rfc822 address

 Args: name  -- possible login name on local system

 Result: returns NULL or pointer to static string rfc822 address.
  ----*/
char *
local_name_lookup(name)
    char *name;
{
    struct passwd *pw;
    static char    buf[100];

    pw = getpwnam(name);
    if(pw == NULL)
      return(NULL);

    strcpy(buf, gcos_name(pw->pw_gecos, name));

    return(buf);
}


