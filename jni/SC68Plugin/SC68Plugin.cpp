#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <android/log.h> 
#include <jni.h>

#include "misc.h" 

/* sc68 includes */
#include <sc68/sc68.h>
#include <sc68/file68_rsc.h>
#include <sc68/file68.h>
#include <unice68.h>

#include "com_ssb_droidsound_plugins_SC68Plugin.h"


#define INFO_TITLE      0
#define INFO_AUTHOR     1
#define INFO_LENGTH     2
#define INFO_TYPE       3
#define INFO_COPYRIGHT  4
#define INFO_GAME       5
#define INFO_SUBTUNES   6
#define INFO_STARTTUNE  7

#define INFO_ASID 101
#define INFO_RIPPER 102
#define INFO_CONVERTER 103
#define INFO_RATE       104 
#define INFO_FORMAT     105

#define SC68_OPT_ASID 1

#define INFO_INSTRUMENTS  100
#define INFO_CHANNELS     101
#define INFO_PATTERNS     102

#define OPT_ASID_FILTER 1000


static char data_dir[1024];

static jstring NewString(JNIEnv *env, const char *str)
{
	static jchar *temp, *ptr;

	temp = (jchar *) malloc((strlen(str) + 1) * sizeof(jchar));
	ptr = temp;
    
	while(*str)
    {
		unsigned char c = (unsigned char)*str++;
		*ptr++ = (c < 0x7f && c >= 0x20) || c >= 0xa0 || c == 0xa ? c : '?';
	}
	//*ptr++ = 0;
	jstring j = env->NewString(temp, ptr - temp);

	free(temp);

	return j;
}


struct PlayData
{
	_sc68_s *sc68; 
    sc68_music_info_t info;
    int currentTrack;
    int defaultTrack;
    bool finished;
};


static void write_debug(int level, void * cookie, const char * fmt, va_list list)
{
    __android_log_vprint(ANDROID_LOG_VERBOSE, "SC68Debug", fmt, list);
}

JNIEXPORT jlong JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1load(JNIEnv *env, jobject obj, jbyteArray bArray, jint size)
{

	sc68_init_t init68;
	
	memset(&init68, 0, sizeof(init68));

	char * argv[1];
	argv[0] = "sc68";
	
	init68.msg_handler = (sc68_msg_t)write_debug;
	init68.debug_set_mask = -1;
	init68.argc = 1;
	init68.argv = argv;
	
	
	if (sc68_init(&init68) != 0) {
		__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "Init failed");
		return 0;
	}
	__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "Init Passed");
	/* Init resets all paths */
	//rsc68_set_share(data_dir);
	rsc68_set_user(data_dir);


    sc68_t *sc68 = sc68_create(NULL);
    if (! sc68) {
        __android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "Create failed");
        return 0;
    }


	int alen = env->GetArrayLength(bArray);
	if (size > alen) {
		__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "Rejected song input, out of bounds");
		return 0;
	}

	jbyte *ptr = env->GetByteArrayElements(bArray, NULL);
	int res = sc68_load_mem(sc68, ptr, size);
	env->ReleaseByteArrayElements(bArray, ptr, 0);
	
	if (res)
	{
		__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "Rejected song input, load_mem failed");
		sc68_destroy(sc68);
		sc68_shutdown();
		return 0;
	}
    
	PlayData *pd = (PlayData*)malloc(sizeof(PlayData));
    pd->sc68 = sc68;
    
	pd->finished = false;
	return (long) pd;
}

JNIEXPORT jlong JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1PlaySong(JNIEnv *env, jobject obj, jlong song, jint track, jint loop_mode)	
{
    PlayData *pd = (PlayData*) song;
    if (pd->finished)
	{
        return 0;
    }

	sc68_play(pd->sc68, track, loop_mode);
	
    if (sc68_process(pd->sc68, NULL, 0) == SC68_ERROR)
	{
        free(pd);
        sc68_destroy(pd->sc68);
        sc68_shutdown();
        return 0;
    }

    pd->currentTrack = pd->sc68->track;
	pd->defaultTrack = pd->sc68->track;
	
    __android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "Default track is %d", pd->defaultTrack);
    
	return (long) pd;
}

JNIEXPORT jint JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1getSoundData(JNIEnv *env, jobject obj, jlong song, jshortArray sArray, jint size)
{
    PlayData *pd = (PlayData*) song;
    if (pd->finished) {
        return -1;
    }

    jshort *ptr = (jshort*) env->GetShortArrayElements(sArray, NULL);
    jint n = size / 2;
    int code = sc68_process(pd->sc68, ptr, &n);
    env->ReleaseShortArrayElements(sArray, ptr, 0);

    if (code == SC68_ERROR) {
        return -1;
    }
    
    if (code & SC68_CHANGE) {
        __android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "Change track");
        pd->finished = true;
    }

    if (code & SC68_END) {
        __android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "End track");
        pd->finished = true;
    }

    return n * 2;
}

JNIEXPORT jlong JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1loadInfo(JNIEnv *env, jobject obj, jbyteArray data, jint size)
{
}

JNIEXPORT void JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1unload(JNIEnv *env, jobject obj, jlong song)
{
    PlayData *pd = (PlayData*)song;
    sc68_destroy(pd->sc68);
    free(pd);
    sc68_shutdown();
}

JNIEXPORT jboolean JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1seekTo(JNIEnv *env, jobject obj, jlong song, jint msec)
{
    /* FIXME: is there a seek function anymore? */
    //PlayData *pd = (PlayData*) song;
    //int status;
    //sc68_seek(pd->sc68, msec, &status);
    return true;
}


JNIEXPORT jboolean JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1setTune(JNIEnv *env, jobject obj, jlong song, jint tune)
{
    PlayData *pd = (PlayData*) song;
    
    pd->currentTrack = tune+1;
	int current_loop_mode = pd->sc68->loop_to;
    sc68_play(pd->sc68, pd->currentTrack, current_loop_mode);
    int code = sc68_process(pd->sc68, 0, 0);
    if (code == SC68_ERROR)
	{
        return false;
    }
    pd->finished = false;
    
    return true;
}


JNIEXPORT jstring JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1getStringInfo(JNIEnv *env, jobject obj, jlong song, jint what)
{
    PlayData *pd = (PlayData*) song;
    if (sc68_music_info(pd->sc68, &pd->info, pd->currentTrack, 0)) {
        return 0;
    }
    
	disk68_t *d;
	static char temp[16]; 
    
    switch(what)
    {
		case INFO_RIPPER:
			for (int i=0; i<pd->info.trk.tags; i++)
			{
				d = (disk68_t*)pd->sc68->disk;
				int result = strcmp("ripper",d->mus[pd->currentTrack - 1].tags.array[i].key);
				if (result == 0)
				{
					return NewString(env, d->mus[pd->currentTrack - 1].tags.array[i].val);
				}

			}
			break;

		case INFO_CONVERTER:
			for (int i=0; i<pd->info.trk.tags; i++)
			{
				d = (disk68_t*)pd->sc68->disk;
				int result = strcmp("converter",d->mus[pd->currentTrack - 1].tags.array[i].key);
				if (result == 0)
				{
					return NewString(env, d->mus[pd->currentTrack - 1].tags.array[i].val);
				}

			}
			break;
	
	
        case INFO_AUTHOR:
            return NewString(env, pd->info.artist);
        case INFO_TITLE:
            return NewString(env, pd->info.title);
        case 51:
            return NewString(env, pd->info.trk.hw);
        case 52:
            return NewString(env, pd->info.replay);
		case INFO_FORMAT:
			return NewString(env, pd->info.format); 			
		case INFO_RATE:
			sprintf(temp, "%dhz", pd->info.rate); 
			return NewString(env, temp); 			
    }
    return 0;
}

JNIEXPORT jint JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1getIntInfo(JNIEnv *env, jobject obj, jlong song, jint what)
{
    PlayData *pd = (PlayData*) song;
    if (sc68_music_info(pd->sc68, &pd->info, pd->currentTrack, 0)) {
        return -1;
    }
    
    sc68_music_info_t &info = pd->info;
    switch(what)
    {
        case INFO_ASID:
			return info.trk.asid;
		case INFO_LENGTH:
            return info.trk.time_ms;
        case INFO_SUBTUNES:
            return info.tracks;
        case INFO_STARTTUNE:
            return pd->defaultTrack-1;
        case 50:
            return (pd->info.trk.amiga << 2) | (pd->info.trk.ste << 1) | (pd->info.trk.ym);
    }
    return -1;
}


JNIEXPORT void JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1setDataDir(JNIEnv *env, jclass klass, jstring dataDir)
{
    const char *filename = env->GetStringUTFChars(dataDir, 0);
    __android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "DataDir: %s", filename);
    strcpy(data_dir, filename);
    env->ReleaseStringUTFChars(dataDir, filename);
}

JNIEXPORT jbyteArray JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1unice(JNIEnv *env, jobject obj, jbyteArray src)
{
    jbyte *sptr = env->GetByteArrayElements(src, NULL);
    int rc = 0;
    int ssize = env->GetArrayLength(src);
    int size = unice68_depacked_size(sptr, NULL);
    
    if (size <= 0) {
        return NULL;
    }

    jbyteArray dest = env->NewByteArray(size);
    jbyte *dptr = env->GetByteArrayElements(dest, NULL);
    int res = unice68_depacker(dptr, sptr);
        
    env->ReleaseByteArrayElements(src, sptr, 0);
    env->ReleaseByteArrayElements(dest, dptr, 0);

    if (res != 0) {
        dest = NULL;
    }
        
    return dest;
}
JNIEXPORT jint JNICALL Java_com_ssb_droidsound_plugins_SC68Plugin_N_1setOption(JNIEnv *env, jclass cl, jlong song, jint option, jint val)
{
    PlayData *pd = (PlayData*) song;
	if (sc68_music_info(pd->sc68, &pd->info, pd->currentTrack, 0))
	{
        return -1;
    }

    int result = -1;
	__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "setOption");
	switch(option)
	{
		case SC68_OPT_ASID:
			__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "aSID SETTING");
		
			if (val == 0)
			{
				result = sc68_cntl(pd->sc68, SC68_SET_ASID, SC68_ASID_OFF);
				__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "DISABLED aSID");
				break;
			}

			if (val == 1)
			{
				result = sc68_cntl(pd->sc68, SC68_CAN_ASID, SC68_DSK_TRACK);
				if (result != -1)
				{
					result = sc68_cntl(pd->sc68, SC68_SET_ASID, result); 
					__android_log_print(ANDROID_LOG_VERBOSE, "SC68Plugin", "ENABLED aSID");
					
				} 
				break;
			}
			
	}
	return result;
}
 