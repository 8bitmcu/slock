/* configuration file location, subdirectory of XDG_CONFIG_HOME */
static const char* slock_cfg = "/slock/slock.cfg";

/* user and group to drop privileges to */
static const char* user  = "nobody";
static const char* group = "nobody";

static const char* colorname[NUMCOLS] = {
  [FOREGROUND] = "#ffffff",
	[INIT] =   "#2d2d2d",     /* after initialization */
	[INPUT] =  "#005577",   /* during input */
	[INPUT_ALT] = "#227799", /* during input, second color */
	[FAILED] = "#CC3333",   /* wrong password */
	[CAPS] = "red",         /* CapsLock on */
	[PAM] =    "#9400D3",   /* waiting for PAM */
};

/* treat a cleared input like a wrong password (color) */
static int failonclear = 1;

/*Enable blur*/
static int enableblur = 0;

/*Set blur radius*/
static int blurradius = 0;

/*Enable Pixelation*/
static int enablepixel = 1;

/*Set pixelation radius*/
static int pixelsize = 8;


/* time in seconds to cancel lock with mouse movement or keyboard input */
static int timetocancel = 3;

static const char* icon_font = "FontAwesome:size=92";
static const char* display_icon = "";

static const char* text_font = "Sans:size=20";
static const char* display_text = "Type password to unlock";

/* background image. Leave empty to use screen-capture */ 
static const char* bgimage = "";

/* number of failed password attempts until failcommand is executed.
   Set to 0 to disable */
static int failcount = 10;

/* command to be executed after [failcount] failed password attempts */
static const char* failcommand = "shutdown -h now";

/* allow control key to trigger fail on clear */
static int controlkeyclear = 0;

/* turn off monitor after a set timeout */ 
static int enabledpms = 1;

/* time in seconds before the monitor shuts down */
static int monitortime = 30;

/* use PAM instead of shadow support */
static int enablepam = 0;

/* PAM service that's used for authentication */
static const char* pam_service = "login";
