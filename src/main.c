/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Kevin DeKorte 2006 <kdekorte@gmail.com>
 *
 * main.c is free software.
 *
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * main.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with main.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <mntent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <libintl.h>
#include <signal.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "common.h"
#include "support.h"
#include "gmtk.h"

static gint reallyverbose;
static gint last_x, last_y;
static gint stored_window_width, stored_window_height;
static gchar *rpname;
static gboolean new_instance;
static gboolean use_pausing_keep_force;
// tv stuff
static gchar *tv_device;
static gchar *tv_driver;
static gchar *tv_input;
static gint tv_width;
static gint tv_height;
static gint tv_fps;

typedef struct _PlayData {
    gchar uri[4096];
    gboolean playlist;
} PlayData;

static GOptionEntry entries[] = {
    {"width", 'w', 0, G_OPTION_ARG_INT, &window_x, N_("Width of window to embed in"), "X"},
    {"height", 'h', 0, G_OPTION_ARG_INT, &window_y, N_("Height of window to embed in"), "Y"},
    {"playlist", 0, 0, G_OPTION_ARG_NONE, &playlist, N_("File Argument is a playlist"), NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, N_("Show more output on the console"), NULL},
    {"reallyverbose", '\0', 0, G_OPTION_ARG_NONE, &reallyverbose,
     N_("Show even more output on the console"), NULL},
    {"fullscreen", 0, 0, G_OPTION_ARG_NONE, &init_fullscreen, N_("Start in fullscreen mode"), NULL},
    {"softvol", 0, 0, G_OPTION_ARG_NONE, &softvol, N_("Use mplayer software volume control"), NULL},
    {"remember_softvol", 0, 0, G_OPTION_ARG_NONE, &remember_softvol,
     N_("When set to TRUE the last volume level is set as the default"), NULL},
    {"volume_softvol", 0, 0, G_OPTION_ARG_INT, &volume_softvol,
     N_("Last software volume percentage- only applied when remember_softvol is set to TRUE"),
     NULL},
    {"mixer", 0, 0, G_OPTION_ARG_STRING, &(audio_device.alsa_mixer), N_("Mixer to use"), NULL},
    {"volume", 0, 0, G_OPTION_ARG_INT, &(pref_volume), N_("Set initial volume percentage"), NULL},
    // note that sizeof(gint)==sizeof(gboolean), so we can give &showcontrols here
    {"showcontrols", 0, 0, G_OPTION_ARG_INT, &showcontrols, N_("Show the controls in window"),
     "[0|1]"},
    {"showsubtitles", 0, 0, G_OPTION_ARG_INT, &showsubtitles, N_("Show the subtitles if available"),
     "[0|1]"},
    {"autostart", 0, 0, G_OPTION_ARG_INT, &autostart,
     N_("Autostart the media default to 1, set to 0 to load but don't play"), "[0|1]"},
    {"disablecontextmenu", 0, 0, G_OPTION_ARG_NONE, &disable_context_menu,
     N_("Disable popup menu on right click"), NULL},
    {"loop", 0, 0, G_OPTION_ARG_NONE, &loop, N_("Play all files on the playlist forever"), NULL},
    {"quit_on_complete", 'q', 0, G_OPTION_ARG_NONE, &quit_on_complete,
     N_("Quit application when last file on playlist is played"), NULL},
    {"random", 0, 0, G_OPTION_ARG_NONE, &random_order, N_("Play items on playlist in random order"),
     NULL},
    {"cache", 0, 0, G_OPTION_ARG_INT, &cache_size, N_("Set cache size"),
     NULL},
    {"ss", 0, 0, G_OPTION_ARG_INT, &start_second, N_("Start Second (default to 0)"),
     NULL},
    {"endpos", 0, 0, G_OPTION_ARG_INT, &play_length,
     N_("Length of media to play from start second"),
     NULL},
    {"forcecache", 0, 0, G_OPTION_ARG_NONE, &forcecache, N_("Force cache usage on streaming sites"),
     NULL},
    {"disabledeinterlace", 0, 0, G_OPTION_ARG_NONE, &disable_deinterlace,
     N_("Disable the deinterlace filter"),
     NULL},
    {"disableframedrop", 0, 0, G_OPTION_ARG_NONE, &disable_framedrop,
     N_("Don't skip drawing frames to better keep sync"),
     NULL},
    {"disableass", 0, 0, G_OPTION_ARG_NONE, &disable_ass,
     N_("Use the old subtitle rendering system"),
     NULL},
    {"disableembeddedfonts", 0, 0, G_OPTION_ARG_NONE, &disable_embeddedfonts,
     N_("Don't use fonts embedded on matroska files"),
     NULL},
    {"vertical", 0, 0, G_OPTION_ARG_NONE, &vertical_layout, N_("Use Vertical Layout"),
     NULL},
    {"showplaylist", 0, 0, G_OPTION_ARG_NONE, &playlist_visible, N_("Start with playlist open"),
     NULL},
    {"showdetails", 0, 0, G_OPTION_ARG_NONE, &details_visible, N_("Start with details visible"),
     NULL},
    {"rpname", 0, 0, G_OPTION_ARG_STRING, &rpname, N_("Real Player Name"), "NAME"},
    {"rpconsole", 0, 0, G_OPTION_ARG_STRING, &rpconsole, N_("Real Player Console ID"), "CONSOLE"},
    {"rpcontrols", 0, 0, G_OPTION_ARG_STRING, &rpcontrols, N_("Real Player Console Controls"),
     "Control Name,..."},
    {"subtitle", 0, 0, G_OPTION_ARG_STRING, &subtitle, N_("Subtitle file for first media file"),
     "FILENAME"},
    {"tvdevice", 0, 0, G_OPTION_ARG_STRING, &tv_device, N_("TV device name"), "DEVICE"},
    {"tvdriver", 0, 0, G_OPTION_ARG_STRING, &tv_driver, N_("TV driver name (v4l|v4l2)"), "DRIVER"},
    {"tvinput", 0, 0, G_OPTION_ARG_STRING, &tv_input, N_("TV input name"), "INPUT"},
    {"tvwidth", 0, 0, G_OPTION_ARG_INT, &tv_width, N_("Width of TV input"), "WIDTH"},
    {"tvheight", 0, 0, G_OPTION_ARG_INT, &tv_height, N_("Height of TV input"), "HEIGHT"},
    {"tvfps", 0, 0, G_OPTION_ARG_INT, &tv_fps, N_("Frames per second from TV input"), "FPS"},
    {"single_instance", 0, 0, G_OPTION_ARG_NONE, &single_instance, N_("Only allow one instance"),
     NULL},
    {"replace_and_play", 0, 0, G_OPTION_ARG_NONE, &replace_and_play,
     N_("Put single instance mode into replace and play mode"),
     NULL},
    {"large_buttons", 0, 0, G_OPTION_ARG_NONE, &large_buttons,
     N_("Use large control buttons"),
     NULL},
    {"always_hide_after_timeout", 0, 0, G_OPTION_ARG_NONE, &always_hide_after_timeout,
     N_("Hide control panel when mouse is not moving"),
     NULL},
    {"new_instance", 0, 0, G_OPTION_ARG_NONE, &new_instance,
     N_("Ignore single instance preference for this instance"),
     NULL},
    {"keep_on_top", 0, 0, G_OPTION_ARG_NONE, &keep_on_top,
     N_("Keep window on top"),
     NULL},
    {"disable_cover_art_fetch", 0, 0, G_OPTION_ARG_NONE, &disable_cover_art_fetch,
     N_("Don't fetch new cover art images"),
     NULL},
    {"dvd_device", 0, 0, G_OPTION_ARG_STRING, &option_dvd_device, N_("DVD Device Name"), "Path to device or folder"},
    {"vo", 0, 0, G_OPTION_ARG_STRING, &option_vo, N_("Video Output Device Name"), "mplayer vo name"},
    {"mplayer", 0, 0, G_OPTION_ARG_STRING, &mplayer_bin, N_("Use specified mplayer"), "Path to mplayer binary"},
    {NULL}
};

gboolean async_play_iter(void *data)
{
    next_iter = (GtkTreeIter *) (data);
    gm_log(verbose, G_LOG_LEVEL_DEBUG, "media state = %s",
           gmtk_media_state_to_string(gmtk_media_player_get_media_state(GMTK_MEDIA_PLAYER(media))));
    if (gmtk_media_player_get_media_state(GMTK_MEDIA_PLAYER(media)) == MEDIA_STATE_UNKNOWN) {
        play_iter(next_iter, 0);
        next_iter = NULL;
    }

    return FALSE;
}

gboolean play(void *data)
{
    PlayData *p = (PlayData *) data;

    if (ok_to_play && p != NULL) {
        if (!gtk_list_store_iter_is_valid(playliststore, &iter)) {
            gm_log(verbose, G_LOG_LEVEL_DEBUG, "iter is not valid, getting first one");
            gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playliststore), &iter);
        }
        gtk_list_store_set(playliststore, &iter, PLAYLIST_COLUMN, p->playlist, ITEM_COLUMN, p->uri, -1);
        play_iter(&iter, 0);
    }
    g_free(p);

    return FALSE;
}


void play_next()
{
    gchar *filename;
    gint count;
    PlayData *p = NULL;

    if (next_item_in_playlist(&iter)) {
        if (gtk_list_store_iter_is_valid(playliststore, &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(playliststore), &iter, ITEM_COLUMN, &filename,
                               COUNT_COLUMN, &count, PLAYLIST_COLUMN, &playlist, -1);
            g_strlcpy(idledata->info, filename, sizeof(idledata->info));
            g_idle_add(set_title_bar, idledata);
            p = (PlayData *) g_malloc(sizeof(PlayData));
            g_strlcpy(p->uri, filename, sizeof(p->uri));
            p->playlist = playlist;
            g_idle_add(play, p);
            g_free(filename);
        }
    } else {
        gm_log(verbose, G_LOG_LEVEL_DEBUG, "end of thread playlist is empty");
        if (loop) {
            if (is_first_item_in_playlist(&iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(playliststore), &iter, ITEM_COLUMN,
                                   &filename, COUNT_COLUMN, &count, PLAYLIST_COLUMN, &playlist, -1);
                g_strlcpy(idledata->info, filename, sizeof(idledata->info));
                g_idle_add(set_title_bar, idledata);
                p = (PlayData *) g_malloc(sizeof(PlayData));
                g_strlcpy(p->uri, filename, sizeof(p->uri));
                p->playlist = playlist;
                g_idle_add(play, p);
                g_free(filename);
            }
        } else {
            idledata->fullscreen = 0;
            g_idle_add(set_fullscreen, idledata);
            g_idle_add(set_stop, idledata);
        }

        if (quit_on_complete) {
            g_idle_add(set_quit, idledata);
        }
    }
}


gint play_iter(GtkTreeIter * playiter, gint restart_second)
{

    gchar *subtitle = NULL;
    gchar *audiofile = NULL;
    GtkTreePath *path;
    gchar *uri = NULL;
    gint count;
    gboolean playlist;
    gchar *title = NULL;
    gchar *artist = NULL;
    gchar *album = NULL;
    gchar *audio_codec;
    gchar *video_codec = NULL;
    GtkAllocation alloc;
    gchar *demuxer = NULL;
    gboolean playable = TRUE;
    gint width;
    gint height;
    gfloat length_value;
    gint i;
    gchar *cover_art_file = NULL;
    gchar *buffer = NULL;
    gchar *message = NULL;
    MetaData *metadata;
    GtkRecentData *recent_data;
#ifdef GIO_ENABLED
    GFile *file;
    GFileInfo *file_info;
#endif

    if (gtk_list_store_iter_is_valid(playliststore, playiter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(playliststore), playiter, ITEM_COLUMN, &uri,
                           DESCRIPTION_COLUMN, &title, LENGTH_VALUE_COLUMN, &length_value,
                           ARTIST_COLUMN, &artist,
                           ALBUM_COLUMN, &album,
                           AUDIO_CODEC_COLUMN, &audio_codec,
                           VIDEO_CODEC_COLUMN, &video_codec,
                           VIDEO_WIDTH_COLUMN, &width,
                           VIDEO_HEIGHT_COLUMN, &height,
                           DEMUXER_COLUMN, &demuxer,
                           COVERART_COLUMN, &cover_art_file,
                           SUBTITLE_COLUMN, &subtitle,
                           AUDIOFILE_COLUMN, &audiofile,
                           COUNT_COLUMN, &count, PLAYLIST_COLUMN, &playlist, PLAYABLE_COLUMN, &playable, -1);
        if (GTK_IS_TREE_SELECTION(selection)) {
            path = gtk_tree_model_get_path(GTK_TREE_MODEL(playliststore), playiter);
            if (path) {
                gtk_tree_selection_select_path(selection, path);
                if (GTK_IS_WIDGET(list))
                    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(list), path, NULL, FALSE, 0, 0);
                buffer = gtk_tree_path_to_string(path);
                g_free(buffer);
                gtk_tree_path_free(path);
            }
        }
        gtk_list_store_set(playliststore, playiter, COUNT_COLUMN, count + 1, -1);
    } else {
        gm_log(verbose, G_LOG_LEVEL_DEBUG, "iter is invalid, nothing to play");
        return 0;
    }

    gm_log(verbose, G_LOG_LEVEL_INFO, "playing - %s", uri);
    gm_log(verbose, G_LOG_LEVEL_INFO, "is playlist %s", gm_bool_to_string(playlist));

    gtk_widget_get_allocation(GTK_WIDGET(media), &alloc);
    if (width == 0 || height == 0) {
        alloc.width = 16;
        alloc.height = 16;
    } else {
        alloc.width = width;
        alloc.height = height;
    }
    gm_log(verbose, G_LOG_LEVEL_DEBUG, "setting window size to %i x %i", alloc.width, alloc.height);
    gtk_widget_size_allocate(GTK_WIDGET(media), &alloc);
    gm_log(verbose, G_LOG_LEVEL_DEBUG, "waiting for all events to drain");
    while (gtk_events_pending())
        gtk_main_iteration();

    // reset audio meter
    for (i = 0; i < METER_BARS; i++) {
        buckets[i] = 0;
        max_buckets[i] = 0;
    }

    gmtk_media_tracker_set_text(tracker, _("Playing"));
    gmtk_media_tracker_set_position(tracker, (gfloat) restart_second);
    gmtk_media_tracker_set_length(tracker, length_value);

    message = g_strdup_printf("<small>\n");
    if (title == NULL) {
        title = g_filename_display_basename(uri);
    }
    buffer = g_markup_printf_escaped("\t<big><b>%s</b></big>\n", title);
    message = g_strconcat(message, buffer, NULL);
    g_free(buffer);

    if (artist != NULL) {
        buffer = g_markup_printf_escaped("\t<i>%s</i>\n", artist);
        message = g_strconcat(message, buffer, NULL);
        g_free(buffer);
    }
    if (album != NULL) {
        buffer = g_markup_printf_escaped("\t%s\n", album);
        message = g_strconcat(message, buffer, NULL);
        g_free(buffer);
    }
    //buffer = g_markup_printf_escaped("\n\t%s\n", uri);
    //message = g_strconcat(message, buffer, NULL);
    //g_free(buffer);

    message = g_strconcat(message, "</small>", NULL);

    // probably not much cover art for random video files
    if (cover_art_file == NULL && video_codec == NULL && !streaming_media(uri) && !playlist) {
        metadata = (MetaData *) g_new0(MetaData, 1);
        metadata->uri = g_strdup(uri);
        if (title != NULL)
            metadata->title = g_strstrip(g_strdup(title));
        if (artist != NULL)
            metadata->artist = g_strstrip(g_strdup(artist));
        if (album != NULL)
            metadata->album = g_strstrip(g_strdup(album));
    } else {
        gtk_image_clear(GTK_IMAGE(cover_art));
    }

    g_strlcpy(idledata->media_info, message, sizeof(idledata->media_info));
    g_strlcpy(idledata->display_name, title, sizeof(idledata->display_name));
    g_free(message);

    message = gm_tempname(NULL, "mplayer-af_exportXXXXXX");
    g_strlcpy(idledata->af_export, message, sizeof(idledata->af_export));
    g_free(message);

    message = g_strdup("");
    if (title == NULL) {
        title = g_filename_display_basename(uri);
    }
    //buffer = g_markup_printf_escaped("\t<b>%s</b>\n", title);
    //message = g_strconcat(message, buffer, NULL);
    //g_free(buffer);

    if (artist != NULL) {
        buffer = g_markup_printf_escaped("\t<i>%s</i>\n", artist);
        message = g_strconcat(message, buffer, NULL);
        g_free(buffer);
    }
    if (album != NULL) {
        buffer = g_markup_printf_escaped("\t%s\n", album);
        message = g_strconcat(message, buffer, NULL);
        g_free(buffer);
    }
    g_strlcpy(idledata->media_notification, message, sizeof(idledata->media_notification));
    g_free(message);

    set_media_label(idledata);

    if (subtitles)
        gtk_container_forall(GTK_CONTAINER(subtitles), remove_langs, NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(menuitem_edit_select_sub_lang), FALSE);
    if (tracks)
        gtk_container_forall(GTK_CONTAINER(tracks), remove_langs, NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(menuitem_edit_select_audio_lang), FALSE);
    lang_group = NULL;
    audio_group = NULL;


    if (subtitle != NULL) {
        gmtk_media_player_set_attribute_string(GMTK_MEDIA_PLAYER(media), ATTRIBUTE_SUBTITLE_FILE, subtitle);
        g_free(subtitle);
        subtitle = NULL;
    }
    if (audiofile != NULL) {
        gmtk_media_player_set_attribute_string(GMTK_MEDIA_PLAYER(media), ATTRIBUTE_AUDIO_TRACK_FILE, audiofile);
        g_free(audiofile);
        audiofile = NULL;
    }

#ifdef GIO_ENABLED
    // don't put it on the recent list, if it is running in plugin mode
    if (!streaming_media(uri)) {
        recent_data = (GtkRecentData *) g_new0(GtkRecentData, 1);
        if (artist != NULL && strlen(artist) > 0) {
            recent_data->display_name = g_strdup_printf("%s - %s", artist, title);
        } else {
            recent_data->display_name = g_strdup(title);
        }
        g_strlcpy(idledata->display_name, recent_data->display_name, sizeof(idledata->display_name));


        file = g_file_new_for_uri(uri);
        file_info = g_file_query_info(file,
                                      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                      G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, G_FILE_QUERY_INFO_NONE, NULL, NULL);


        if (file_info) {
            recent_data->mime_type = g_strdup(g_file_info_get_content_type(file_info));
            g_object_unref(file_info);
        }
        g_object_unref(file);
        recent_data->app_name = g_strdup("wgmplayer");
        recent_data->app_exec = g_strdup("wgmplayer %u");
        if (recent_data->mime_type != NULL) {
            gtk_recent_manager_add_full(recent_manager, uri, recent_data);
            g_free(recent_data->mime_type);
        }
        g_free(recent_data->app_name);
        g_free(recent_data->app_exec);
        g_free(recent_data);

    }
#endif
    g_free(title);
    g_free(artist);
    g_free(album);
    if (demuxer != NULL) {
        g_strlcpy(idledata->demuxer, demuxer, sizeof(idledata->demuxer));
        g_free(demuxer);
    } else {
        g_strlcpy(idledata->demuxer, "", sizeof(idledata->demuxer));
    }

    last_x = 0;
    last_y = 0;
    idledata->width = width;
    idledata->height = height;

    idledata->retry_on_full_cache = FALSE;
    idledata->cachepercent = -1.0;
    g_strlcpy(idledata->info, uri, sizeof(idledata->info));
    set_title_bar(idledata);

    streaming = 0;

    gm_store = gm_pref_store_new("wgmplayer");
    forcecache = gm_pref_store_get_boolean(gm_store, FORCECACHE);
    gm_pref_store_free(gm_store);


    if (g_ascii_strcasecmp(uri, "dvdnav://") == 0) {
        gtk_widget_show(menu_event_box);
    } else {
        gtk_widget_hide(menu_event_box);
    }

    if (autostart) {
        g_idle_add(hide_buttons, idledata);
        js_state = STATE_PLAYING;

        if (g_str_has_prefix(uri, "mmshttp") || g_str_has_prefix(uri, "http") || g_str_has_prefix(uri, "mms")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_NETWORK);
        } else if (g_str_has_prefix(uri, "dvd") || g_str_has_prefix(uri, "dvdnav")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_DVD);
        } else if (g_str_has_prefix(uri, "cdda")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_CD);
        } else if (g_str_has_prefix(uri, "cddb")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_CD);
        } else if (g_str_has_prefix(uri, "vcd")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_VCD);
        } else if (g_str_has_prefix(uri, "tv")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_TV);
        } else if (g_str_has_prefix(uri, "dvb")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_DVB);
        } else if (g_str_has_prefix(uri, "file")) {
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_FILE);
        } else {
            // if all else fails it must be a network type
            gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_NETWORK);
        }

        gmtk_media_player_set_attribute_boolean(GMTK_MEDIA_PLAYER(media), ATTRIBUTE_PLAYLIST, playlist);
        gmtk_media_player_set_uri(GMTK_MEDIA_PLAYER(media), uri);
        gmtk_media_player_set_state(GMTK_MEDIA_PLAYER(media), MEDIA_STATE_PLAY);

    }

    return 0;
}

static void hup_handler(int signum)
{
    gm_log(verbose, G_LOG_LEVEL_DEBUG, "handling signal %i", signum);
    delete_callback(NULL, NULL, NULL);
    g_idle_add(set_destroy, NULL);
}

void assign_default_keys()
{
    if (!accel_keys[FILE_OPEN_LOCATION]) {
        accel_keys[FILE_OPEN_LOCATION] = g_strdup("4+l");
    }
    if (!accel_keys[EDIT_SCREENSHOT]) {
        accel_keys[EDIT_SCREENSHOT] = g_strdup("4+t");
    }
    if (!accel_keys[EDIT_PREFERENCES]) {
        accel_keys[EDIT_PREFERENCES] = g_strdup("4+p");
    }
    if (!accel_keys[VIEW_PLAYLIST]) {
        accel_keys[VIEW_PLAYLIST] = g_strdup("F9");
    }
    if (!accel_keys[VIEW_INFO]) {
        accel_keys[VIEW_INFO] = g_strdup("i");
    }
    if (!accel_keys[VIEW_DETAILS]) {
        accel_keys[VIEW_DETAILS] = g_strdup("4+d");
    }
    if (!accel_keys[VIEW_METER]) {
        accel_keys[VIEW_METER] = g_strdup("4+m");
    }
    if (!accel_keys[VIEW_FULLSCREEN]) {
        accel_keys[VIEW_FULLSCREEN] = g_strdup("4+f");
    }
    if (!accel_keys[VIEW_ASPECT]) {
        accel_keys[VIEW_ASPECT] = g_strdup("a");
    }
    if (!accel_keys[VIEW_SUBTITLES]) {
        accel_keys[VIEW_SUBTITLES] = g_strdup("v");
    }
    if (!accel_keys[VIEW_DECREASE_SIZE]) {
        accel_keys[VIEW_DECREASE_SIZE] = g_strdup("1+R");
    }
    if (!accel_keys[VIEW_INCREASE_SIZE]) {
        accel_keys[VIEW_INCREASE_SIZE] = g_strdup("1+T");
    }
    if (!accel_keys[VIEW_ANGLE]) {
        accel_keys[VIEW_ANGLE] = g_strdup("4+a");
    }
    if (!accel_keys[VIEW_CONTROLS]) {
        accel_keys[VIEW_CONTROLS] = g_strdup("c");
    }
}

int main(int argc, char *argv[])
{
    struct stat buf = { 0 };
    struct mntent *mnt = NULL;
    FILE *fp = NULL;
    gchar *uri;
    gint fileindex = 1;
    GError *error = NULL;
    GOptionContext *context;
    guint i;
    gdouble volume = 100.0;
    gchar *accelerator_keys;
    gchar **parse;
#if GTK_MAJOR_VERSION >= 3
    GtkSettings *gtk_settings;
#endif
    int stat_result;

    struct sigaction sa;
    gboolean playiter = FALSE;

#ifdef GIO_ENABLED
    GFile *file;
#endif

#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    playlist = FALSE;
    window_x = 0;
    window_y = 0;
    last_window_width = 0;
    last_window_height = 0;
    showcontrols = TRUE;
    showsubtitles = TRUE;
    autostart = 1;
    videopresent = FALSE;
    disable_context_menu = FALSE;
    dontplaynext = FALSE;
    idledata = (IdleData *) g_new0(IdleData, 1);
    idledata->videopresent = FALSE;
    idledata->length = 0.0;
    idledata->device = NULL;
    idledata->cachepercent = -1.0;
    selection = NULL;
    path = NULL;
    js_state = STATE_UNDEFINED;
    control_instance = TRUE;
    playlistname = NULL;
    rpconsole = NULL;
    subtitle = NULL;
    tv_device = NULL;
    tv_driver = NULL;
    tv_width = 0;
    tv_height = 0;
    tv_fps = 0;
    ok_to_play = TRUE;
    alang = NULL;
    slang = NULL;
    metadata_codepage = NULL;
    playlistname = NULL;
    window_width = -1;
    window_height = -1;
    stored_window_width = -1;
    stored_window_height = -1;
    cache_size = 0;
    forcecache = FALSE;
    vertical_layout = FALSE;
    playlist_visible = FALSE;
    disable_framedrop = FALSE;
    softvol = TRUE;
    remember_softvol = TRUE;
    volume_softvol = 0.8;
    volume_gain = 0;
    subtitlefont = NULL;
    subtitle_codepage = NULL;
    subtitle_color = NULL;
    subtitle_outline = FALSE;
    subtitle_shadow = FALSE;
    subtitle_fuzziness = 0;
    disable_embeddedfonts = FALSE;
    quit_on_complete = FALSE;
    verbose = 0;
    reallyverbose = 0;
    disable_pause_on_click = FALSE;
    disable_animation = FALSE;
    disable_cover_art_fetch = FALSE;
    auto_hide_timeout = 3;
    mouse_over_controls = FALSE;
    use_mediakeys = TRUE;
    use_defaultpl = FALSE;
    mplayer_bin = NULL;
    mplayer_dvd_device = NULL;
    single_instance = FALSE;
    disable_deinterlace = TRUE;
    details_visible = FALSE;
    replace_and_play = FALSE;
    bring_to_front = FALSE;
    keep_on_top = FALSE;
    resize_on_new_media = FALSE;
    use_pausing_keep_force = FALSE;
    show_notification = TRUE;
    show_status_icon = TRUE;
    lang_group = NULL;
    audio_group = NULL;
    disable_cover_art_fetch = FALSE;
    fullscreen = FALSE;
    vo = NULL;
    data = NULL;
    max_data = NULL;
    details_table = NULL;
    large_buttons = FALSE;
    button_size = GTK_ICON_SIZE_BUTTON;
    lastguistate = -1;
    non_fs_height = 0;
    non_fs_width = 0;
    use_hw_audio = FALSE;
    start_second = 0;
    play_length = 0;
    save_loc = TRUE;

    update_control_flag = FALSE;
    skip_fixed_allocation_on_show = FALSE;
    skip_fixed_allocation_on_hide = FALSE;
    pref_volume = -1;
    cover_art_uri = NULL;
    resume_mode = RESUME_ALWAYS_ASK;

    use_hardware_codecs = FALSE;
    use_crystalhd_codecs = FALSE;
    disable_ass = FALSE;

#if GTK_MAJOR_VERSION == 2
    if (!g_thread_supported())
        g_thread_init(NULL);
#endif

    g_type_init();
    gtk_init(&argc, &argv);
    g_setenv("PULSE_PROP_media.role", "video", TRUE);

    sa.sa_handler = hup_handler;
    sigemptyset(&sa.sa_mask);
#ifdef SA_RESTART
    sa.sa_flags = SA_RESTART;   /* Restart functions if
                                   interrupted by handler */
#endif

    gm_log_name_this_thread("root");

#ifdef SIGINT
    if (sigaction(SIGINT, &sa, NULL) == -1)
        gm_log(verbose, G_LOG_LEVEL_MESSAGE, "SIGINT signal handler not installed");
#endif
#ifdef SIGHUP
    if (sigaction(SIGHUP, &sa, NULL) == -1)
        gm_log(verbose, G_LOG_LEVEL_MESSAGE, "SIGHUP signal handler not installed");
#endif
#ifdef SIGTERM
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        gm_log(verbose, G_LOG_LEVEL_MESSAGE, "SIGTERM signal handler not installed");
#endif

    uri = g_strdup_printf("%s/wgmplayer/cover_art", g_get_user_config_dir());
    if (!g_file_test(uri, G_FILE_TEST_IS_DIR)) {
        g_mkdir_with_parents(uri, 0775);
    }
    g_free(uri);

    uri = g_strdup_printf("%s/wgmplayer/plugin", g_get_user_config_dir());
    if (!g_file_test(uri, G_FILE_TEST_IS_DIR)) {
        g_mkdir_with_parents(uri, 0775);
    }
    g_free(uri);
    uri = NULL;

    default_playlist = g_strdup_printf("file://%s/wgmplayer/default.pls", g_get_user_config_dir());
    safe_to_save_default_playlist = TRUE;

    gm_store = gm_pref_store_new("wgmplayer");
    vo = gm_pref_store_get_string(gm_store, VO);
    audio_device.alsa_mixer = gm_pref_store_get_string(gm_store, ALSA_MIXER);
    use_hardware_codecs = gm_pref_store_get_boolean_with_default (gm_store, USE_HARDWARE_CODECS, use_hardware_codecs);
    use_crystalhd_codecs = gm_pref_store_get_boolean_with_default (gm_store, USE_CRYSTALHD_CODECS, use_crystalhd_codecs);
    osdlevel = gm_pref_store_get_int(gm_store, OSDLEVEL);
    pplevel = gm_pref_store_get_int(gm_store, PPLEVEL);
#ifndef HAVE_ASOUNDLIB
    volume = gm_pref_store_get_int(gm_store, VOLUME);
    if (pref_volume == -1) {
        pref_volume = volume;
    }
#endif
    audio_channels = gm_pref_store_get_int(gm_store, AUDIO_CHANNELS);
    use_hw_audio = gm_pref_store_get_boolean_with_default (gm_store, USE_HW_AUDIO, use_hw_audio);
    fullscreen = gm_pref_store_get_boolean_with_default (gm_store, FULLSCREEN, fullscreen);
    softvol = gm_pref_store_get_boolean_with_default (gm_store, SOFTVOL, softvol);
    remember_softvol = gm_pref_store_get_boolean_with_default (gm_store, REMEMBER_SOFTVOL, remember_softvol);
    volume_softvol = gm_pref_store_get_float_with_default(gm_store, VOLUME_SOFTVOL, volume_softvol);
    volume_gain = gm_pref_store_get_int(gm_store, VOLUME_GAIN);
    forcecache = gm_pref_store_get_boolean_with_default (gm_store, FORCECACHE, forcecache);
    vertical_layout = gm_pref_store_get_boolean_with_default (gm_store, VERTICAL, vertical_layout);
    playlist_visible = gm_pref_store_get_boolean_with_default (gm_store, SHOWPLAYLIST, playlist_visible);
    details_visible = gm_pref_store_get_boolean_with_default (gm_store, SHOWDETAILS, details_visible);
    show_notification = gm_pref_store_get_boolean_with_default (gm_store, SHOW_NOTIFICATION, show_notification);
    show_status_icon = gm_pref_store_get_boolean_with_default (gm_store, SHOW_STATUS_ICON, show_status_icon);
    showcontrols = gm_pref_store_get_boolean_with_default(gm_store, SHOW_CONTROLS, showcontrols);
    restore_controls = showcontrols;
    disable_deinterlace = gm_pref_store_get_boolean_with_default(gm_store, DISABLEDEINTERLACE, disable_deinterlace);
    disable_framedrop = gm_pref_store_get_boolean_with_default(gm_store, DISABLEFRAMEDROP, disable_framedrop);
    disable_context_menu = gm_pref_store_get_boolean_with_default (gm_store, DISABLECONTEXTMENU, disable_context_menu);
    disable_ass = gm_pref_store_get_boolean_with_default (gm_store, DISABLEASS, disable_ass);
    disable_embeddedfonts = gm_pref_store_get_boolean_with_default (gm_store, DISABLEEMBEDDEDFONTS, disable_embeddedfonts);
    disable_pause_on_click = gm_pref_store_get_boolean_with_default (gm_store, DISABLEPAUSEONCLICK, disable_pause_on_click);
    disable_animation = gm_pref_store_get_boolean_with_default (gm_store, DISABLEANIMATION, disable_animation);
    disable_cover_art_fetch =
        gm_pref_store_get_boolean_with_default(gm_store, DISABLE_COVER_ART_FETCH, disable_cover_art_fetch);
    auto_hide_timeout = gm_pref_store_get_int_with_default(gm_store, AUTOHIDETIMEOUT, auto_hide_timeout);
    use_mediakeys = gm_pref_store_get_boolean_with_default(gm_store, USE_MEDIAKEYS, use_mediakeys);
    use_defaultpl = gm_pref_store_get_boolean_with_default(gm_store, USE_DEFAULTPL, use_defaultpl);
    metadata_codepage = gm_pref_store_get_string(gm_store, METADATACODEPAGE);

    alang = gm_pref_store_get_string(gm_store, AUDIO_LANG);
    slang = gm_pref_store_get_string(gm_store, SUBTITLE_LANG);

    subtitlefont = gm_pref_store_get_string(gm_store, SUBTITLEFONT);
    subtitle_scale = gm_pref_store_get_float(gm_store, SUBTITLESCALE);
    if (subtitle_scale < 0.25) {
        subtitle_scale = 1.0;
    }
    subtitle_codepage = gm_pref_store_get_string(gm_store, SUBTITLECODEPAGE);
    subtitle_color = gm_pref_store_get_string(gm_store, SUBTITLECOLOR);
    subtitle_outline = gm_pref_store_get_boolean_with_default (gm_store, SUBTITLEOUTLINE, subtitle_outline);
    subtitle_shadow = gm_pref_store_get_boolean_with_default (gm_store, SUBTITLESHADOW, subtitle_shadow);
    subtitle_margin = gm_pref_store_get_int(gm_store, SUBTITLE_MARGIN);
    subtitle_fuzziness = gm_pref_store_get_int(gm_store, SUBTITLE_FUZZINESS);
    showsubtitles = gm_pref_store_get_boolean_with_default(gm_store, SHOW_SUBTITLES, showsubtitles);

    resume_mode = gm_pref_store_get_int(gm_store, RESUME_MODE);

    single_instance = gm_pref_store_get_boolean_with_default (gm_store, SINGLE_INSTANCE, single_instance);
    if (single_instance) {
        replace_and_play = gm_pref_store_get_boolean_with_default (gm_store, REPLACE_AND_PLAY, replace_and_play);
        bring_to_front = gm_pref_store_get_boolean_with_default (gm_store, BRING_TO_FRONT, bring_to_front);
    }

    if (mplayer_bin == NULL)
        mplayer_bin = gm_pref_store_get_string(gm_store, MPLAYER_BIN);
    if (mplayer_bin != NULL && !g_file_test(mplayer_bin, G_FILE_TEST_EXISTS)) {
        g_free(mplayer_bin);
        mplayer_bin = NULL;
    }
    mplayer_dvd_device = gm_pref_store_get_string(gm_store, MPLAYER_DVD_DEVICE);
    extraopts = gm_pref_store_get_string(gm_store, EXTRAOPTS);
    accelerator_keys = gm_pref_store_get_string(gm_store, ACCELERATOR_KEYS);
    accel_keys = g_strv_new(KEY_COUNT);
    accel_keys_description = g_strv_new(KEY_COUNT);
    if (accelerator_keys != NULL) {
        parse = g_strsplit(accelerator_keys, " ", KEY_COUNT);
        for (i = 0; i < g_strv_length(parse); i++) {
            accel_keys[i] = g_strdup(parse[i]);
        }
        g_free(accelerator_keys);
        g_strfreev(parse);
    }
    assign_default_keys();
    accel_keys_description[FILE_OPEN_LOCATION] = g_strdup(_("Open Location"));
    accel_keys_description[EDIT_SCREENSHOT] = g_strdup(_("Take Screenshot"));
    accel_keys_description[EDIT_PREFERENCES] = g_strdup(_("Preferences"));
    accel_keys_description[VIEW_PLAYLIST] = g_strdup(_("Playlist"));
    accel_keys_description[VIEW_INFO] = g_strdup(_("Media Info"));
    accel_keys_description[VIEW_DETAILS] = g_strdup(_("Details"));
    accel_keys_description[VIEW_METER] = g_strdup(_("Audio Meter"));
    accel_keys_description[VIEW_FULLSCREEN] = g_strdup(_("Full Screen"));
    accel_keys_description[VIEW_ASPECT] = g_strdup(_("Aspect"));
    accel_keys_description[VIEW_SUBTITLES] = g_strdup(_("Subtitles"));
    accel_keys_description[VIEW_DECREASE_SIZE] = g_strdup(_("Decrease Subtitle Size"));
    accel_keys_description[VIEW_INCREASE_SIZE] = g_strdup(_("Increase Subtitle Size"));
    accel_keys_description[VIEW_ANGLE] = g_strdup(_("Switch Angle"));
    accel_keys_description[VIEW_CONTROLS] = g_strdup(_("Controls"));
    remember_loc = gm_pref_store_get_boolean(gm_store, REMEMBER_LOC);
    loc_window_x = gm_pref_store_get_int(gm_store, WINDOW_X);
    loc_window_y = gm_pref_store_get_int(gm_store, WINDOW_Y);
    loc_window_height = gm_pref_store_get_int(gm_store, WINDOW_HEIGHT);
    loc_window_width = gm_pref_store_get_int(gm_store, WINDOW_WIDTH);
    loc_panel_position = gm_pref_store_get_int(gm_store, PANEL_POSITION);
    keep_on_top = gm_pref_store_get_boolean(gm_store, KEEP_ON_TOP);
    resize_on_new_media = gm_pref_store_get_boolean(gm_store, RESIZE_ON_NEW_MEDIA);
    mouse_wheel_changes_volume = gm_pref_store_get_boolean_with_default(gm_store, MOUSE_WHEEL_CHANGES_VOLUME, FALSE);
    audio_device_name = gm_pref_store_get_string(gm_store, AUDIO_DEVICE_NAME);
    audio_device.description = g_strdup(audio_device_name);
    context = g_option_context_new(_("[FILES...] - G-Media player based on MPlayer"));

    g_option_context_set_translation_domain(context, "UTF-8");
    g_option_context_set_translate_func(context, (GTranslateFunc) gettext, NULL, NULL);

    g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);
    if (new_instance)
        single_instance = FALSE;
    if (verbose == 0)
        verbose = gm_pref_store_get_int(gm_store, VERBOSE);
    if (reallyverbose)
        verbose = 2;
    if (verbose) {
        printf(_("WGmplayer v%s\n"), VERSION);
        printf("GTK %i.%i.%i\n", GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
        printf("GLIB %i.%i.%i\n", GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
        printf("GDA Enabled\n");
    }

    if (cache_size == 0)
        cache_size = gm_pref_store_get_int(gm_store, CACHE_SIZE);
    if (cache_size == 0)
        cache_size = 2000;
    gm_pref_store_free(gm_store);

    if (single_instance) {
        gm_log(verbose, G_LOG_LEVEL_INFO, "Running in single instance mode");
    }
#ifdef GIO_ENABLED
    gm_log(verbose, G_LOG_LEVEL_INFO, "Running with GIO support");
#endif
#ifdef ENABLE_PANSCAN
    gm_log(verbose, G_LOG_LEVEL_INFO, "Running with panscan enabled (mplayer svn r29565 or higher required)");
#endif
    gm_log(verbose, G_LOG_LEVEL_INFO, "Using audio device: %s", audio_device_name);

    if (softvol) {
        gm_log(verbose, G_LOG_LEVEL_INFO, "Using MPlayer Software Volume control");
        if (remember_softvol && volume_softvol != -1) {
            gm_log(verbose, G_LOG_LEVEL_INFO, "Using last volume of %f%%", volume_softvol * 100.0);
            volume = (gdouble) volume_softvol *100.0;
            audio_device.volume = volume / 100.0;
        } else {
            volume = 100.0;
        }
    }

    if (large_buttons)
        button_size = GTK_ICON_SIZE_DIALOG;
    if (error != NULL) {
        printf("%s\n", error->message);
        printf(_("Run 'wgmplayer --help' to see a full list of available command line options.\n"));
        return 1;
    }
    gm_log(verbose, G_LOG_LEVEL_DEBUG, "Threading support enabled = %s", gm_bool_to_string(g_thread_supported()));
    if (rpconsole == NULL)
        rpconsole = g_strdup("NONE");
    // setup playliststore
    playliststore =
        gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN,
                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_FLOAT, G_TYPE_STRING,
                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                           G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
                           G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_BOOLEAN);
    // only use dark theme if not embedded, otherwise use the default theme  
#if GTK_MAJOR_VERSION >= 3
    gtk_settings = gtk_settings_get_default();
    g_object_set(G_OBJECT(gtk_settings), "gtk-application-prefer-dark-theme", TRUE, NULL);
#endif

    create_window();
    autopause = FALSE;

#ifdef GIO_ENABLED
    Wg_mutex_init( &(idledata->caching) );
#if GLIB_CHECK_VERSION (2, 32, 0)
    g_cond_init( &(idledata->caching_complete) );
#else
    idledata->caching_complete = g_cond_new ();
#endif
#endif

    retrieve_metadata_pool = g_thread_pool_new(retrieve_metadata, NULL, 10, TRUE, NULL);

    Wg_mutex_init(&retrieve_mutex);
    Wg_mutex_init(&set_mutex);

    if (argv[fileindex] != NULL) {
#ifdef GIO_ENABLED
        file = g_file_new_for_commandline_arg(argv[fileindex]);
        stat_result = -1;
        if (file != NULL) {
            GError *error = NULL;
            GFileInfo *file_info = g_file_query_info(file, G_FILE_ATTRIBUTE_UNIX_MODE, 0, NULL, &error);
            if (file_info != NULL) {
                buf.st_mode = g_file_info_get_attribute_uint32(file_info, G_FILE_ATTRIBUTE_UNIX_MODE);
                stat_result = 0;
                g_object_unref(file_info);
            }
            if (error != NULL) {
                gm_log(verbose, G_LOG_LEVEL_INFO, "failed to get mode: %s", error->message);
                g_error_free(error);
            }
            g_object_unref(file);
        }
#else
        stat_result = g_stat(argv[fileindex], &buf);
#endif
        gm_log(verbose, G_LOG_LEVEL_INFO, "opening %s", argv[fileindex]);
        gm_log(verbose, G_LOG_LEVEL_INFO, "stat_result = %i", stat_result);
        gm_log(verbose, G_LOG_LEVEL_INFO, "is block %s", gm_bool_to_string(S_ISBLK(buf.st_mode)));
        gm_log(verbose, G_LOG_LEVEL_INFO, "is character %s", gm_bool_to_string(S_ISCHR(buf.st_mode)));
        gm_log(verbose, G_LOG_LEVEL_INFO, "is reg %s", gm_bool_to_string(S_ISREG(buf.st_mode)));
        gm_log(verbose, G_LOG_LEVEL_INFO, "is dir %s", gm_bool_to_string(S_ISDIR(buf.st_mode)));
        gm_log(verbose, G_LOG_LEVEL_INFO, "playlist %s", gm_bool_to_string(playlist));
        if (stat_result == 0 && S_ISBLK(buf.st_mode)) {
            // might have a block device, so could be a DVD

#ifdef HAVE_SYS_MOUNT_H
            fp = setmntent("/etc/mtab", "r");
            do {
                mnt = getmntent(fp);
                if (mnt)
                    gm_log(verbose, G_LOG_LEVEL_MESSAGE, "%s is at %s", mnt->mnt_fsname, mnt->mnt_dir);
                if (argv[fileindex] != NULL && mnt && mnt->mnt_fsname != NULL) {
                    if (strcmp(argv[fileindex], mnt->mnt_fsname) == 0)
                        break;
                }
            }
            while (mnt);
            endmntent(fp);
#endif
            if (mnt && mnt->mnt_dir) {
                gm_log(verbose, G_LOG_LEVEL_MESSAGE, "%s is mounted on %s", argv[fileindex], mnt->mnt_dir);
                uri = g_strdup_printf("%s/VIDEO_TS", mnt->mnt_dir);
                stat(uri, &buf);
                g_free(uri);
                if (S_ISDIR(buf.st_mode)) {
                    add_item_to_playlist("dvdnav://", FALSE);
                    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playliststore), &iter);
                    gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_DVD);
                    //play_iter(&iter, 0);
                    playiter = TRUE;
                } else {
                    uri = g_strdup_printf("file://%s", mnt->mnt_dir);
                    create_folder_progress_window();
                    add_folder_to_playlist_callback(uri, NULL);
                    g_free(uri);
                    destroy_folder_progress_window();
                    if (random_order) {
                        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playliststore), &iter);
                        randomize_playlist(playliststore);
                    }
                    if (first_item_in_playlist(playliststore, &iter)) {
                        // play_iter(&iter, 0);
                        playiter = TRUE;
                    }
                }
            } else {
                parse_cdda("cdda://");
                if (random_order) {
                    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playliststore), &iter);
                    randomize_playlist(playliststore);
                }
                //play_file("cdda://", playlist);
                if (first_item_in_playlist(playliststore, &iter)) {
                    // play_iter(&iter, 0);
                    playiter = TRUE;
                }
            }
        } else if (stat_result == 0 && S_ISDIR(buf.st_mode)) {
            uri = g_strdup_printf("%s/VIDEO_TS", argv[fileindex]);
            stat_result = g_stat(uri, &buf);
            g_free(uri);
            if (stat_result == 0 && S_ISDIR(buf.st_mode)) {
                add_item_to_playlist("dvdnav://", FALSE);
                gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playliststore), &iter);
                gmtk_media_player_set_media_type(GMTK_MEDIA_PLAYER(media), TYPE_DVD);
                //play_iter(&iter, 0);
                playiter = TRUE;
            } else {
                create_folder_progress_window();
                uri = NULL;
#ifdef GIO_ENABLED
                file = g_file_new_for_commandline_arg(argv[fileindex]);
                if (file != NULL) {
                    uri = g_file_get_uri(file);
                    g_object_unref(file);
                }
#else
                uri = g_filename_to_uri(argv[fileindex], NULL, NULL);
#endif
                add_folder_to_playlist_callback(uri, NULL);
                g_free(uri);
                destroy_folder_progress_window();
                if (random_order) {
                    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playliststore), &iter);
                    randomize_playlist(playliststore);
                }
                if (first_item_in_playlist(playliststore, &iter)) {
                    //play_iter(&iter, 0);
                    playiter = TRUE;
                }
            }
        } else {
            // local file
            // detect if playlist here, so even if not specified it can be picked up
            i = fileindex;
            while (argv[i] != NULL) {
                gm_log(verbose, G_LOG_LEVEL_DEBUG, "Argument %i is %s", i, argv[i]);
#ifdef GIO_ENABLED
                if (!device_name(argv[i])) {
                    file = g_file_new_for_commandline_arg(argv[i]);
                    if (file != NULL) {
                        uri = g_file_get_uri(file);
                        g_object_unref(file);
                    } else {
                        uri = g_strdup(argv[i]);
                    }
                } else {
                    uri = g_strdup(argv[i]);
                }
#else
                uri = g_filename_to_uri(argv[i], NULL, NULL);
#endif
                if (uri != NULL) {
                    if (playlist == FALSE)
                        playlist = detect_playlist(uri);
                    if (!playlist) {
                        add_item_to_playlist(uri, playlist);
                    } else {
                        if (!parse_playlist(uri)) {
                            add_item_to_playlist(uri, playlist);
                        }
                    }
                    g_free(uri);
                }
                i++;
            }

            if (random_order) {
                gtk_tree_model_get_iter_first(GTK_TREE_MODEL(playliststore), &iter);
                randomize_playlist(playliststore);
            }
            if (first_item_in_playlist(playliststore, &iter)) {
                playiter = TRUE;
            }
        }

    }

    gm_audio_update_device(&audio_device);
    // disabling this line seems to help with hangs on startup when using pulseaudio
    //gm_audio_get_volume(&audio_device);
    set_media_player_attributes(media);
    if (softvol) {
        if (pref_volume != -1) {
            audio_device.volume = (gdouble) pref_volume / 100.0;
            gm_log(verbose, G_LOG_LEVEL_INFO, "The volume on '%s' is %f", audio_device.description,
                   audio_device.volume);
            volume = audio_device.volume * 100;
        } else {
            audio_device.volume = volume / 100.0;
        }
    }
    gm_log(verbose, G_LOG_LEVEL_DEBUG, "Volume is %lf Audio Device Volume = %f", volume, audio_device.volume);

    gtk_scale_button_set_value(GTK_SCALE_BUTTON(vol_slider), audio_device.volume);
    show_window();
    if (playiter)
        play_iter(&iter, 0);
    if (argv[fileindex] == NULL) {
        use_remember_loc = remember_loc;
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem_view_playlist), playlist_visible);
    } else {
        // prevents saving of a playlist with one item on it
        use_defaultpl = FALSE;
        // don't save the loc when launched with a single file
        save_loc = FALSE;
    }

    if (single_instance) {
        use_remember_loc = remember_loc;
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem_view_playlist), playlist_visible);
    }

    if (remember_loc) {
        gtk_window_move(GTK_WINDOW(window), loc_window_x, loc_window_y);
        g_idle_add(set_pane_position, NULL);
    }

    safe_to_save_default_playlist = FALSE;
    if (use_defaultpl) {
        create_folder_progress_window();
        parse_playlist(default_playlist);
        destroy_folder_progress_window();
    }
    safe_to_save_default_playlist = TRUE;
    // put the request to update the volume into the list of tasks to complete
    g_idle_add(hookup_volume, NULL);
    g_idle_add(set_volume, NULL);
    if (fullscreen)
        g_idle_add(set_fullscreen, NULL);
    gtk_main();
    return 0;
}
