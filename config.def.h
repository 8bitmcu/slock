
/* user and group to drop privileges to */
static const char *user  = "nobody";
static const char *group = "nobody";

static const char *colorname[NUMCOLS] = {
  [FOREGROUND] = "#ffffff",
	[INIT] =   "#2d2d2d",     /* after initialization */
	[INPUT] =  "#005577",   /* during input */
	[INPUT_ALT] = "#227799", /* during input, second color */
	[FAILED] = "#CC3333",   /* wrong password */
	[CAPS] = "red",         /* CapsLock on */
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/*Enable blur*/
static const int enable_blur = 0;
/*Set blur radius*/
static const int blurRadius = 0;

/*Enable Pixelation*/
static const int enable_pixel = 1;
/*Set pixelation radius*/
static const int pixelSize = 8;


/* time in seconds to cancel lock with mouse movement */
static const int timetocancel = 2;

static const char *text_font = "San Francisco:size=20";
static const char *display_text = "Type password to unlock";

static const char *icon_font = "FontAwesome:size=92";
static const char *display_icon = "";

/* number of failed password attempts until failcommand is executed.
   Set to 0 to disable */
static const int failcount = 10;

/* command to be executed after [failcount] failed password attempts */
static const char *failcommand = "shutdown -h now";

/* allow control key to trigger fail on clear */
static const int controlkeyclear = 0;

/* time in seconds before the monitor shuts down */
static const int monitortime = 30;
