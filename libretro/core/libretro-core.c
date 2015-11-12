#include "libretro.h"

#include "libretro-core.h"

#ifndef NO_LIBCO
cothread_t mainThread;
cothread_t emuThread;
#else
//extern void quit_vice_emu();
#endif

int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH ;
int retrow=1024; 
int retroh=1024;

extern int SHIFTON,pauseg,SND ,snd_sampler;
extern short signed int SNDBUF[1024*2];
extern char RPATH[512];
extern char RETRO_DIR[512];
extern int vice_statusbar;
extern void set_drive_type(int drive,int val);
extern void set_truedrive_emultion(int val);

#include "cmdline.c"

extern void update_input(void);
extern void texture_init(void);
extern void texture_uninit(void);
extern void Emu_init();
extern void Emu_uninit();
extern void input_gui(void);

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_data_directory[512];;

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {

      {
         "vice_resolution",
         "Internal resolution; 320x240|384x272|384x288|400x300|640x480|832x576|800x600|960x720|1024x768|1024x1024",

      },
      {
         "Statusbar",
         "Status Bar; disabled|enabled",
      },
      {
         "vice_Drive8Type",
         "Drive8Type; 1540|1541|1542|1551|1570|1571|1573|1581|2000|4000|2031|2040|3040|4040|1001|8050|8250",
      },
      {
         "vice_DriveTrueEmulation",
         "DriveTrueEmulation; disabled|enabled",
      },
      { NULL, NULL },
   };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

static void update_variables(void)
{
/*
   struct retro_variable var = {
      .key = "Skel_resolution",
   };
*/

   struct retro_variable var;

   var.key = "vice_resolution";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), var.value);

      pch = strtok(str, "x");
      if (pch)
         retrow = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         retroh = strtoul(pch, NULL, 0);

	//FIXME remove force 384x272
	retrow=WINDOW_WIDTH;
	retroh=WINDOW_HEIGHT;

      fprintf(stderr, "[libretro-vice]: Got size: %u x %u.\n", retrow, retroh);

      CROP_WIDTH =retrow;
      CROP_HEIGHT= (retroh-80);
      VIRTUAL_WIDTH = retrow;
      texture_init();
      //reset_screen();
   }

   var.key = "Statusbar";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0)
         vice_statusbar=1;
      if (strcmp(var.value, "disabled") == 0)
         vice_statusbar=0;
   }
   else
      vice_statusbar=0;

   var.key = "vice_Drive8Type";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char str[100];
	  int val;
      snprintf(str, sizeof(str), var.value);
      val = strtoul(str, NULL, 0);
	  set_drive_type(8, val);
   }

   var.key = "vice_DriveTrueEmulation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "enabled") == 0)
         set_truedrive_emultion(1);
      if (strcmp(var.value, "disabled") == 0)
         set_truedrive_emultion(0);
   }

}

static void retro_wrap_emulator()
{    

   pre_main(RPATH);
#ifndef NO_LIBCO
LOGI("EXIT EMU THD\n");
   pauseg=-1;

   //environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0); 

   // Were done here
   co_switch(mainThread);

   // Dead emulator, but libco says not to return
   while(true)
   {
      LOGI("Running a dead emulator.");
      co_switch(mainThread);
   }
#endif
}

void Emu_init(){

#ifdef RETRO_AND
   MOUSEMODE=1;
#endif

   update_variables();

   memset(Key_Sate,0,512);
   memset(Key_Sate2,0,512);

#ifndef NO_LIBCO
   if(!emuThread && !mainThread)
   {
      mainThread = co_active();
      emuThread = co_create(65536*sizeof(void*), retro_wrap_emulator);
   }
#else
	retro_wrap_emulator();
#endif

}

void Emu_uninit(){
#ifdef NO_LIBCO
	//quit_vice_emu();
#endif
   texture_uninit();
}

void retro_shutdown_core(void)
{
   LOGI("SHUTDOWN\n");
//main_exit();
#ifdef NO_LIBCO
	//quit_vice_emu();
#endif
   texture_uninit();
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

void retro_reset(void){
//      machine_trigger_reset(MACHINE_RESET_MODE_SOFT);
}

void retro_init(void)
{    	
   const char *system_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      // if defined, use the system directory			
      retro_system_directory=system_dir;		
   }		   

   const char *content_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory			
      retro_content_directory=content_dir;		
   }			

   const char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;      
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   if(retro_system_directory==NULL)sprintf(RETRO_DIR, "%s\0",".");
   else sprintf(RETRO_DIR, "%s\0", retro_system_directory);

   sprintf(retro_system_data_directory, "%s/data\0",RETRO_DIR);

   LOGI("Retro SYSTEM_DIRECTORY %s\n",retro_system_directory);
   LOGI("Retro SAVE_DIRECTORY %s\n",retro_save_directory);
   LOGI("Retro CONTENT_DIRECTORY %s\n",retro_content_directory);

#ifndef RENDER16B
    	enum retro_pixel_format fmt =RETRO_PIXEL_FORMAT_XRGB8888;
#else
    	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#endif
   
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "PIXEL FORMAT is not supported.\n");
LOGI("PIXEL FORMAT is not supported.\n");
      exit(0);
   }

	struct retro_input_descriptor inputDescriptors[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" }
	};
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);
#ifndef NO_LIBCO
   Emu_init();
#endif
   texture_init();

}

extern void main_exit();
void retro_deinit(void)
{	 
   Emu_uninit(); 

#ifndef NO_LIBCO
   co_switch(emuThread);
LOGI("exit emu\n");
  // main_exit();
   co_switch(mainThread);
LOGI("exit main\n");
   if(emuThread)
   {	 
      co_delete(emuThread);
      emuThread = 0;
   }
#endif

   LOGI("Retro DeInit\n");
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "Vice";
   info->library_version  = "2.4.20";
   info->valid_extensions = "d64|d71|d80|d81|d82|g64||g41|x64|t64|tap|prg|p00|crt|bin|zip|gz|d6z|d7z|d8z|g6z|g4z|x6z";
//"zip|d64|d71|d80|d82|g64|g41|x64|T64|tap|prg|p00|crt";
   info->need_fullpath    = true;
   info->block_extract = false;

}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
//FIXME handle vice PAL/NTSC
   struct retro_game_geometry geom = { retrow, retroh, 1024, 1024,4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_audiocb(signed short int *sound_buffer,int sndbufsize){
 	int x;
    if(pauseg==0)for(x=0;x<sndbufsize;x++)audio_cb(sound_buffer[x],sound_buffer[x]);	
}


#ifdef NO_LIBCO
//FIXME nolibco Gui endless loop -> no retro_run() call
void retro_run_gui(void)
{
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();
  
   video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);
}
#endif

void retro_run(void)
{
   int x;

   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);

#ifndef NO_LIBCO   
   co_switch(emuThread);
#endif

}
/*
unsigned int lastdown,lastup,lastchar;
static void keyboard_cb(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   //logging.log(RETRO_LOG_INFO, "Down: %s, Code: %d, Char: %u, Mod: %u.\n",
     //    down ? "yes" : "no", keycode, character, mod);
if(down)lastdown=keycode;
else lastup=keycode;
lastchar=character;
}
*/

bool retro_load_game(const struct retro_game_info *info)
{
   const char *full_path;

   (void)info;

/*
   struct retro_keyboard_callback cb = { keyboard_cb };
   environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cb);
*/
   full_path = info->path;

   strcpy(RPATH,full_path);

   update_variables();

#ifdef RENDER16B
	memset(Retro_Screen,0,1024*1024*2);
#else
	memset(Retro_Screen,0,1024*1024*2*2);
#endif
	memset(SNDBUF,0,1024*2*2);

#ifndef NO_LIBCO
	co_switch(emuThread);
#else
	Emu_init();
#endif
   return true;
}

void retro_unload_game(void){

   pauseg=-1;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

