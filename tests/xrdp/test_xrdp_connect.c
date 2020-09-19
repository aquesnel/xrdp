/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright 2020 Alexandre Quesnel
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include <errno.h>
#include <sys/select.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/channels.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/event.h>
//#include <freerdp/settings.h>

// #define ARCH_H
// #include "os_calls.h"

/*****************************************************************************/
// Callbacks

void 
tf_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
}

void 
tf_context_free(freerdp* instance, rdpContext* context)
{

}

int 
tf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChannels* channels = (rdpChannels*) user_data;

	freerdp_channels_load_plugin(channels, settings, name, plugin_data);

	return 1;
}

int 
tf_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

boolean 
tf_pre_connect(freerdp* instance)
{
	rdpSettings* settings;

	settings = instance->settings;

// 	settings->os_major_type = OSMAJORTYPE_UNIX;
// 	settings->os_minor_type = OSMINORTYPE_NATIVE_XSERVER;

    instance->settings->num_channels = 0;
    instance->settings->username = strdup("xrdp-integration-test");
    instance->settings->password = strdup("xrdp-integration-test");
    instance->settings->domain = strdup("");

    
    settings->order_support[NEG_DSTBLT_INDEX] = false;
    settings->order_support[NEG_PATBLT_INDEX] = false;
    settings->order_support[NEG_SCRBLT_INDEX] = false;
    settings->order_support[NEG_OPAQUE_RECT_INDEX] = false;
    settings->order_support[NEG_DRAWNINEGRID_INDEX] = false;
    settings->order_support[NEG_MULTIDSTBLT_INDEX] = false;
    settings->order_support[NEG_MULTIPATBLT_INDEX] = false;
    settings->order_support[NEG_MULTISCRBLT_INDEX] = false;
    settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = false;
    settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = false;
    settings->order_support[NEG_LINETO_INDEX] = false;
    settings->order_support[NEG_POLYLINE_INDEX] = false;
    settings->order_support[NEG_MEMBLT_INDEX] = false;
    settings->order_support[NEG_MEM3BLT_INDEX] = false;
    settings->order_support[NEG_MEMBLT_V2_INDEX] = false;
    settings->order_support[NEG_MEM3BLT_V2_INDEX] = false;
    settings->order_support[NEG_SAVEBITMAP_INDEX] = false;
    settings->order_support[NEG_GLYPH_INDEX_INDEX] = false;
    settings->order_support[NEG_FAST_INDEX_INDEX] = false;
    settings->order_support[NEG_FAST_GLYPH_INDEX] = false;
    settings->order_support[NEG_POLYGON_SC_INDEX] = false;
    settings->order_support[NEG_POLYGON_CB_INDEX] = false;
    settings->order_support[NEG_ELLIPSE_SC_INDEX] = false;
    settings->order_support[NEG_ELLIPSE_CB_INDEX] = false;

	freerdp_channels_pre_connect(instance->context->channels, instance);
    return true;
}

boolean 
tf_post_connect(freerdp* instance)
{
    return true;
}

/*****************************************************************************/
freerdp*
test_rdp_client_init()
{
    // https://pub.freerdp.com/api/freerdp_8h.html#a44b7878cf0f61ce42bcf3a34a1d2b368
//  freerdp_get_version(&(mod->vmaj), &(mod->vmin), &(mod->vrev));

    
// 	pthread_t thread;
// 	freerdp* instance;
// 	struct thread_data* data;
// 	rdpChannels* channels;

	freerdp_channels_global_init();

// 	g_sem = freerdp_sem_new(1);
    freerdp *instance = freerdp_new();
	instance->PreConnect = tf_pre_connect;
	instance->PostConnect = tf_post_connect;
	instance->ReceiveChannelData = tf_receive_channel_data;

	instance->context_size = 0;
	instance->ContextNew = tf_context_new;
	instance->ContextFree = tf_context_free;
	freerdp_context_new(instance);

    int argc = 2;
	char* argv[2];
	argv[0] = strdup("test_xrdp_connect");
	argv[1] = strdup("127.0.0.1");
	if (freerdp_parse_args(instance->settings, argc, argv, tf_process_plugin_args, instance->context->channels, NULL, NULL) < 0)
	{
		printf("failed to parse arguments.\n");

		exit(1);
	}
	return instance;
}


/*****************************************************************************/
int
test_rdp_client_connect(freerdp* instance)
{
    return freerdp_connect(instance);
}

/*****************************************************************************/
int
test_rdp_client_event_loop(freerdp* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	rdpChannels* channels;
	struct timeval timeout;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));
	memset(&timeout, 0, sizeof(struct timeval));

	channels = instance->context->channels;
	
	for(int select_requests = 0; select_requests < 1000 && !freerdp_shall_disconnect(instance); select_requests++)
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			return 1;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != true)
		{
			printf("Failed to get channel manager file descriptor\n");
			return 2;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			return 3;

        timeout.tv_sec = 5;
		if (select(max_fds + 1, &rfds_set, &wfds_set, NULL, &timeout) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("tfreerdp_run: select failed\n");
				return 4;
			}
		}

		if (freerdp_check_fds(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			return 5;
		}
		if (freerdp_channels_check_fds(channels, instance) != true)
		{
			printf("Failed to check channel manager file descriptor\n");
			return 6;
		}

    	RDP_EVENT* event = freerdp_channels_pop_event(channels);
    	if (event)
    	{
    		switch (event->event_type)
    		{
    			default:
    				break;
    		}
    
    		freerdp_event_free(event);
    	}
	}
	
	return 0;
}

/*****************************************************************************/
void
test_rdp_client_disconnect(freerdp* instance)
{
	freerdp_channels_close(instance->context->channels, instance);
	freerdp_channels_free(instance->context->channels);
	freerdp_disconnect(instance);
}

void
test_rdp_client_deinit(freerdp* instance)
{
	freerdp_free(instance);
	freerdp_channels_global_uninit();
}

/*****************************************************************************/
int
main(int argc, char **argv)
{
    freerdp* freerdp_client = test_rdp_client_init();
    test_rdp_client_connect(freerdp_client);
    test_rdp_client_event_loop(freerdp_client);
    test_rdp_client_disconnect(freerdp_client);
    test_rdp_client_deinit(freerdp_client);
    printf("end of test.\n");
    return 0;
}
