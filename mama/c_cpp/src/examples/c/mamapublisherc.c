/* $Id$
 *
 * OpenMAMA: The open middleware agnostic messaging API
 * Copyright (C) 2011 NYSE Technologies, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "wombat/port.h"

#include "mama/mama.h"
#include "string.h"
#include <stdio.h>

static mamaTransport    gTransport      = NULL;
static mamaTimer        gTimer          = NULL;
static mamaSubscription gSubscription   = NULL;
static mamaPublisher    gPublisher      = NULL;
static mamaQueue        gQueue          = NULL;
static mamaDispatcher   gDispatcher     = NULL;
static mamaMsgCallbacks gInboundCb;

static mamaBridge       gMamaBridge         = NULL;
static mamaQueue        gMamaDefaultQueue   = NULL;
static const char*      gMiddleware         = "wmw";

static const char *     gOutBoundTopic  = "MAMA_TOPIC";
static const char *     gInBoundTopic   = "MAMA_INBOUND_TOPIC";
static const char *     gTransportName  = "pub";
static double           gInterval       = 0.5;
static int              gQuietLevel     = 0;
static int              gPubCb          = 0;
static int              gCount          = 0;
static const char *     gUsageString[]  =
{
" This sample application demonstrates how to publish mama messages, and",
" respond to requests from a clients inbox.",
"",
" It accepts the following command line arguments:",
"      [-s topic]         The topic on which to send messages. Default is",
"                         is \"MAMA_TOPIC\".",
"      [-l topic]         The topic on which to listen for inbound requests.",
"                         Default is \"MAMA_INBOUND_TOPIC\".",
"      [-i interval]      The interval between messages .Default in  0.5.",
"      [-tport name]      The transport parameters to be used from",
"                         mama.properties. Default is pub",
"      [-m middleware]    The middleware to use [wmw/lbm/tibrv].",
"                         Default is wmw.",
"      [-q]               Quiet mode. Suppress output.",
"      [-v]               Increase verbosity. Can be passed multiple times",
"      [-pubCb]           Listen for publisher callbacks",
NULL
};

static void parseCommandLine    (int argc, const char **argv);
static void initializeMama      (void);
static void createIntervalTimer (void);
static void createInboundSubscription (void);

static void MAMACALLTYPE timerCallback       (mamaTimer timer, void *closure);
static void MAMACALLTYPE inboundCreateCb     (mamaSubscription subscription, void *closure);

static void MAMACALLTYPE
inboundErrorCb      (mamaSubscription subscription,
                     mama_status status,
                     void* platformError,
                     const char *subject,
                     void *closure);

static void MAMACALLTYPE
inboundMsgCb        (mamaSubscription subscription,
                     mamaMsg msg,
                     void *closure,
                     void *itemClosure);

static void MAMACALLTYPE
publisherOnCreateCb (mamaPublisher publisher,
                     void*         closure);

static void MAMACALLTYPE
publisherOnDestroyCb (mamaPublisher publisher,
                      void*         closure);

static void MAMACALLTYPE
publisherOnErrorCb (mamaPublisher publisher,
                    mama_status   status,
                    const char*   info,
                    void*         closure);

static void publishMessage      (mamaMsg request);
static void createPublisher     (void);
static void usage               (int exitStatus);


int main (int argc, const char **argv)
{
    setbuf (stdout, NULL);
    parseCommandLine (argc, argv);

    initializeMama ();

    createIntervalTimer ();

    createInboundSubscription ();

    createPublisher ();

    mama_start (gMamaBridge);

    mamaDispatcher_destroy (gDispatcher);
    mamaQueue_destroy (gQueue);

    mama_close();

    return 0;
}

void initializeMama (void)
{
    mama_status status;

    status = mama_loadBridge (&gMamaBridge, gMiddleware);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error loading bridge: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }

    status = mama_open ();
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error initializing mama: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }

    /*Use the default internal event queue for all subscriptions and timers*/
    mama_getDefaultEventQueue (gMamaBridge, &gMamaDefaultQueue);

    /* Get a queue for the publisher */
    status = mamaQueue_create (&gQueue, gMamaBridge);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error allocating queue: %s\n", mamaStatus_stringForStatus (status));
        exit (status);
    }

    /* Get a dispatcher for the queue */
    status = mamaDispatcher_create (&gDispatcher, gQueue);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error allocating dispatcher: %s\n", mamaStatus_stringForStatus (status));
        exit (status);
    }

    status = mamaTransport_allocate (&gTransport);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error allocating transport: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }

    status = mamaTransport_create (gTransport, gTransportName, gMamaBridge);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error creating transport: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }
}

static void MAMACALLTYPE publisherOnCreateCb (
                         mamaPublisher publisher,
                         void*         closure)
{
    if (gQuietLevel < 1)
    {
        const char* symbol = "";
        mamaPublisher_getSymbol (publisher, &symbol);
        mama_log(MAMA_LOG_LEVEL_NORMAL, "publisherOnCreateCb: %s", symbol);
    }
}

static void MAMACALLTYPE publisherOnDestroyCb (
                         mamaPublisher publisher,
                         void*         closure)
{
    if (gQuietLevel < 1)
    {
        const char* symbol = "";
        mamaPublisher_getSymbol (publisher, &symbol);
        mama_log(MAMA_LOG_LEVEL_NORMAL, "publisherOnDestroyCb: %s", symbol);
    }
}

static void MAMACALLTYPE publisherOnErrorCb (
                         mamaPublisher publisher,
                         mama_status   status,
                         const char*   info,
                         void*         closure)
{
    if (gQuietLevel < 1)
    {
        const char* symbol = "";
        mamaPublisher_getSymbol (publisher, &symbol);
        mama_log(MAMA_LOG_LEVEL_NORMAL, "publisherOnErrorCb: %s status=%d/%s info=%s",
            symbol, status, mamaStatus_stringForStatus(status), info);
    }
}

static void MAMACALLTYPE publisherOnSuccessCb (
                         mamaPublisher publisher,
                         mama_status   status,
                         const char*   info,
                         void*         closure)
{
    if (gQuietLevel < 1)
    {
        const char* symbol = "";
        mamaPublisher_getSymbol(publisher, &symbol);
        mama_log(MAMA_LOG_LEVEL_FINEST, "publisherOnSuccessCb: %s status=%d/%s info=%s",
            symbol, status, mamaStatus_stringForStatus(status), info);
    }
}

static void createPublisher (void)
{
    mama_status status;

    if (gPubCb)
    {
        mamaPublisherCallbacks* cb = NULL;
        mamaPublisherCallbacks_allocate (&cb);
        cb->onCreate = publisherOnCreateCb;
        cb->onError = publisherOnErrorCb;
        cb->onSuccess = publisherOnSuccessCb;
        cb->onDestroy = publisherOnDestroyCb;
        status = mamaPublisher_createWithCallbacks (&gPublisher,
                                                    gTransport,
                                                    gQueue,
                                                    gOutBoundTopic,
                                                    NULL,   /* Not needed for basic publishers */
                                                    NULL,   /* Not needed for basic publishers */
                                                    cb,
                                                    NULL);
        mamaPublisherCallbacks_deallocate (cb);
    }
    else
    {
        status = mamaPublisher_create (&gPublisher,
                                       gTransport,
                                       gOutBoundTopic,
                                       NULL,   /* Not needed for basic publishers */
                                       NULL); /* Not needed for basic publishers */
    }

    if (status != MAMA_STATUS_OK)
    {
        printf ("Error creating publisher: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }
}

static void publishMessage (mamaMsg request)
{
    static int msgNumber = 1;
    static mamaMsg msg = NULL;
    mamaMsgType msgType;
    mama_status status;

    if (msg == NULL)
    {
        status = mamaMsg_create(&msg);
    }
    else
    {
        status = mamaMsg_clear(msg);
    }

    if (status != MAMA_STATUS_OK)
    {
        printf ("Error creating/clearing msg: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }


    /* Add some fields. This is not required, but illustrates how to
     * send data.
     */
    if (msgNumber == 1) msgType = MAMA_MSG_TYPE_INITIAL;
    else msgType = MAMA_MSG_TYPE_UPDATE;
    status = mamaMsg_addI32 (msg, MamaFieldMsgType.mName, MamaFieldMsgType.mFid, msgType);
    status = mamaMsg_addI32 (msg, MamaFieldMsgStatus.mName, MamaFieldMsgStatus.mFid, MAMA_MSG_STATUS_OK);
    status = mamaMsg_addI32 (msg, MamaFieldSeqNum.mName, MamaFieldSeqNum.mFid, msgNumber);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error adding int to msg: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }

    status = mamaMsg_addString (msg, "MdFeedHost", 12, gOutBoundTopic);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error adding string to msg: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }

    if (request)
    {
        if (gQuietLevel < 1)
        {
            mama_log (MAMA_LOG_LEVEL_NORMAL, "Publishing message %d to inbox .", msgNumber);
        }
        status = mamaMsg_addU8 (msg, MamaFieldMsgType.mName, MamaFieldMsgType.mFid, MAMA_MSG_TYPE_RECAP);
        status = mamaPublisher_sendReplyToInbox (gPublisher, request, msg);
    }
    else
    {
        if (gQuietLevel < 1)
        {
           mama_log (MAMA_LOG_LEVEL_NORMAL, "Publishing message %d to %s {%s}",
               msgNumber, gOutBoundTopic, mamaMsg_toString (msg));
        }
        status = mamaPublisher_send (gPublisher, msg);
    }

    if (status != MAMA_STATUS_OK)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR, "Error sending msg: %s\n",
                mamaStatus_stringForStatus (status));
        if (status != MAMA_STATUS_NOT_ENTITLED)
        {
            exit (status);
        }
    }
    msgNumber++;
}

static void createInboundSubscription (void)
{
    mama_status status;

    memset(&gInboundCb, 0, sizeof(gInboundCb));
    gInboundCb.onCreate         = inboundCreateCb;
    gInboundCb.onError          = inboundErrorCb;
    gInboundCb.onMsg            = inboundMsgCb;
    gInboundCb.onQuality        = NULL; /* not used by basic subscriptions */
    gInboundCb.onGap            = NULL; /* not used by basic subscriptions */
    gInboundCb.onRecapRequest   = NULL; /* not used by basic subscriptions */

    status = mamaSubscription_allocate (&gSubscription);

    status = mamaSubscription_createBasic (gSubscription,
                                           gTransport,
                                           gMamaDefaultQueue,
                                           &gInboundCb,
                                           gInBoundTopic,
                                           NULL);
    if (status != MAMA_STATUS_OK)
    {
        printf ("Error creating subscription: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }
}

static void MAMACALLTYPE
inboundCreateCb (mamaSubscription subscription, void *closure)
{
    if (gQuietLevel < 2)
    {
        printf ("Created inbound subscription.\n");
    }
}

static void MAMACALLTYPE
inboundErrorCb (mamaSubscription subscription,
                mama_status      status,
                void*            platformError,
                const char*      subject,
                void*            closure)
{
    printf ("Error creating subscription: %s\n",
            mamaStatus_stringForStatus (status));
    exit (status);
}

static void MAMACALLTYPE
inboundMsgCb (mamaSubscription subscription,
              mamaMsg          msg,
              void*            closure,
              void*            itemClosure)
{

    if (gQuietLevel < 2)
    {
        printf ("Recieved inbound msg. (%s) Sending response\n", mamaMsg_toString (msg));
    }

    if (!mamaMsg_isFromInbox (msg))
    {
        printf ("Inbound msg not from inbox. No reply sent.\n");
        return;
    }

    publishMessage (msg);
}

static void createIntervalTimer (void)
{
    mama_status status;

    status = mamaTimer_create (&gTimer,
                               gMamaDefaultQueue,
                               timerCallback,
                               gInterval,
                               NULL);

    if (status != MAMA_STATUS_OK)
    {
        printf ("Error creating timer: %s\n",
                mamaStatus_stringForStatus (status));
        exit (status);
    }
}


static void MAMACALLTYPE
timerCallback (mamaTimer timer, void *closure)
{
    static int count = 0;

    publishMessage (NULL);
    if (gCount && ++count >= gCount)
    {
        mamaPublisher_destroy (gPublisher);
        sleep (1);    /* to see all queued events */
        mama_stop (gMamaBridge);
    }
}

void parseCommandLine (int argc, const char **argv)
{
    int i = 0;
    for (i = 1; i < argc; )
    {
        if (strcmp ("-s", argv[i]) == 0)
        {
            gOutBoundTopic = argv[i+1];
            i += 2;
        }
        else if (strcmp ("-l", argv[i]) == 0)
        {
            gInBoundTopic = argv[i+1];
            i += 2;
        }
        else if ((strcmp (argv[i], "-h") == 0)||
                 (strcmp (argv[i], "--help") == 0)||
                 (strcmp (argv[i], "-?") == 0))
        {
            usage(0);
        }
        else if (strcmp ("-i", argv[i]) == 0)
        {
            gInterval = strtod (argv[i+1], NULL);
            i += 2;
        }
        else if (strcmp ("-c", argv[i]) == 0)
        {
            gCount = strtod (argv[i+1], NULL);
            i += 2;
        }
        else if (strcmp ("-tport", argv[i]) == 0)
        {
            gTransportName = argv[i+1];
            i += 2;
        }
        else if (strcmp ("-q", argv[i]) == 0)
        {
            gQuietLevel++;
            i++;
        }
        else if (strcmp ("-pubCb", argv[i]) == 0)
        {
            gPubCb = 1;
            i++;
        }
        else if (strcmp ("-m", argv[i]) == 0)
        {
            gMiddleware = argv[i+1];
            i += 2;
        }
        else if (strcmp (argv[i], "-v") == 0)
        {
            if ( mama_getLogLevel () == MAMA_LOG_LEVEL_WARN )
            {
                mama_enableLogging( stderr, MAMA_LOG_LEVEL_NORMAL );
            }
            else if ( mama_getLogLevel () == MAMA_LOG_LEVEL_NORMAL )
            {
                mama_enableLogging( stderr, MAMA_LOG_LEVEL_FINE );
            }
            else if( mama_getLogLevel () == MAMA_LOG_LEVEL_FINE )
            {
                mama_enableLogging( stderr, MAMA_LOG_LEVEL_FINER );
            }
            else
            {
                mama_enableLogging( stderr, MAMA_LOG_LEVEL_FINEST );
            }
            i++;
        }
        else
        {
            printf ("Unknown arg: %s\n",    argv[i]);
            i++;
        }
    }

    if (gQuietLevel < 2)
    {
        printf ("Starting Publisher with:\n"
                "   topic:              %s\n"
                "   inbound topic:      %s\n"
                "   interval            %f\n"
                "   transport:          %s\n",
                gOutBoundTopic, gInBoundTopic, gInterval, gTransportName);
    }
}

void usage (int exitStatus)
{
    int i=0;
    while (gUsageString[i]!=NULL)
    {
        printf ("%s\n", gUsageString[i++]);
    }
    exit (exitStatus);
}
