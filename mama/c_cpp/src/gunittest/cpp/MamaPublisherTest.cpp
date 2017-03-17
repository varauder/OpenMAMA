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

#include "MamaPublisherTest.h"

MamaPublisherTest::MamaPublisherTest(void)
{
}

MamaPublisherTest::~MamaPublisherTest(void)
{
}

/* ************************************************************************* */
/* Setup and Teardown */
/* ************************************************************************* */

void MamaPublisherTest::SetUp(void)
{   
       // Save the this pointer in the member variable to get around gtest problems
       m_this = this;

       // Load the bridge
       m_bridge = Mama::loadBridge(getMiddleware());

       // Open mama
       Mama::open();

       // Transport
       transportName = getTransport();
    m_transport = new MamaTransport();
       m_transport->create(transportName, m_bridge);
}

void MamaPublisherTest::TearDown(void)
{
       // Destroy the transport
       delete m_transport;

       // Close mama
       Mama::close();

       // Clean the member this pointer
       m_this = NULL;   
}

void MamaPublisherTest::onStartComplete(MamaStatus status)
{
}

/**
 * Publish callback class.
 */
class TestCallback : public MamaPublisherCallback
{
private:
    int onCreateCount;
    int onErrorCount;
    int onSuccessCount;
    int onDestroyCount;

public:

    int getOnCreateCount() { return onCreateCount; }
    int getOnErrorCount() { return onErrorCount; }
    int getOnSuccessCount() { return onSuccessCount; }
    int getOnDestroyCount() { return onDestroyCount; }
    
       TestCallback()
       {
        onCreateCount = 0;
        onErrorCount = 0;
        onSuccessCount = 0;
        onDestroyCount = 0;
       }

       virtual void onCreate (
           MamaPublisher*  publisher,
           void* closure)
       {
         onCreateCount++;
       }

       virtual void onError (
           MamaPublisher*    publisher,
           const MamaStatus& status,
           const char*       info,
           void*             closure)
       {
         onErrorCount++;
       }

       virtual void onSuccess (
           MamaPublisher*    publisher,
           const MamaStatus& status,
           const char*       info,
           void*             closure)
       {
         ++onSuccessCount;
       }

       virtual void onDestroy (
           MamaPublisher*  publisher,
           void*           closure)
       {
         onDestroyCount++;
       }
};

/**
 * Test publisher with create/publish/destroy
 */
TEST_F(MamaPublisherTest, Publish)
{
    int numPublishes = 10;

    // Allocate a publisher
    MamaPublisher *publisher = new MamaPublisher();

    // Create the publisher
    publisher->create(m_transport, getSymbol(), getSource(), NULL);

    // Process messages until the first message is received
    Mama::startBackground(m_bridge, this);

    MamaMsg* msg = new MamaMsg();
    msg->create();
    msg->addU8("", MamaFieldMsgType.mFid, MAMA_MSG_TYPE_INITIAL);
    msg->addU8("", MamaFieldMsgStatus.mFid, MAMA_MSG_STATUS_OK);
    msg->addString("", 11, "TEST STRING");    // MdFeedName

    for (int i = 0; i < numPublishes; ++i)
    {
        publisher->send(msg);
    }
    sleep (1);

    // Destroy the publisher
    publisher->destroy();            

    // Delete the publisher
    delete publisher;
    delete msg;

    Mama::stop(m_bridge);
}

/**
 * Test publish with callbacks
 */
TEST_F(MamaPublisherTest, PublishWithCallbacks)
{
    int numPublishes = 10;
	int waits = 0;

    // Create a callback object
    TestCallback *testCallback = new TestCallback();

    // Allocate a publisher
    MamaPublisher *publisher = new MamaPublisher();

    // Get the default queue
    MamaQueue *queue = Mama::getDefaultEventQueue(m_bridge);

    // Create the publisher
    publisher->createWithCallbacks(m_transport, queue, testCallback, NULL, getSymbol(), getSource(), NULL);

    // Process messages until the first message is received
    Mama::startBackground(m_bridge, this);

    MamaMsg* msg = new MamaMsg();
    msg->create();
    msg->addU8("", MamaFieldMsgType.mFid, MAMA_MSG_TYPE_INITIAL);
    msg->addU8("", MamaFieldMsgStatus.mFid, MAMA_MSG_STATUS_OK);
    msg->addString("", 11, "TEST STRING");    // MdFeedName

    for (int i = 0; i < numPublishes; ++i)
    {
        publisher->send(msg);
    }

    sleep(1);

    // Destroy the publisher
    publisher->destroy();

    // Wait for onDestroy
	while (testCallback->getOnDestroyCount() == 0)
	{
		if (waits++ > 10) break;    /* way too long to wait for onDestroy */
        sleep(1);
	}
    ASSERT_EQ(1, testCallback->getOnDestroyCount());

    delete publisher;
    delete msg;

    Mama::stop(m_bridge);

    ASSERT_EQ(1, testCallback->getOnCreateCount());
    ASSERT_EQ(0, testCallback->getOnErrorCount());
    ASSERT_EQ(1, testCallback->getOnSuccessCount());

    delete testCallback;
}

/**
 * Test publisher with callbacks and errors
 */
TEST_F(MamaPublisherTest, DISABLED_PublishWithCallbacksBadSource)
{
    int numPublishes = 10;
	int waits = 0;

    // Create a callback object
    TestCallback *testCallback = new TestCallback();

    // Allocate a publisher
    MamaPublisher *publisher = new MamaPublisher();

    // Get the default queue
    MamaQueue *queue = Mama::getDefaultEventQueue(m_bridge);

    // Create the publisher
    publisher->createWithCallbacks(m_transport, queue, testCallback, NULL, getSymbol(), getBadSource(), NULL);

    // Process messages until the first message is received
    Mama::startBackground(m_bridge, this);

    MamaMsg* msg = new MamaMsg();
    msg->create();
    msg->addU8("", MamaFieldMsgType.mFid, MAMA_MSG_TYPE_INITIAL);
    msg->addU8("", MamaFieldMsgStatus.mFid, MAMA_MSG_STATUS_OK);
    msg->addString("", 11, "TEST STRING");    // MdFeedName

    for (int i = 0; i < numPublishes; ++i)
    {
        publisher->send(msg);
    }

    // Destroy the publisher
    publisher->destroy();            

    // Wait for onDestroy
	while (testCallback->getOnDestroyCount() == 0)
	{
		if (waits++ > 10) break;    /* way too long to wait for onDestroy */
        sleep(1);
	}
    ASSERT_EQ(1, testCallback->getOnDestroyCount());

    delete publisher;
    delete testCallback;
    delete msg;

    Mama::stop(m_bridge);

    ASSERT_EQ(1, testCallback->getOnCreateCount());
    ASSERT_EQ(numPublishes, testCallback->getOnErrorCount());
    ASSERT_EQ(0, testCallback->getOnSuccessCount());
}

/**
 * Test publisher with NULL callback.
 */
TEST_F(MamaPublisherTest, PublishWithNullCallback)
{
    int numPublishes = 10;

    // Allocate a publisher
    MamaPublisher *publisher = new MamaPublisher();

    // Get the default queue
    MamaQueue *queue = Mama::getDefaultEventQueue(m_bridge);

    // Create the publisher
    publisher->createWithCallbacks(m_transport, queue, NULL, NULL, getSymbol(), getBadSource(), NULL);

    // Process messages until the first message is received
    Mama::startBackground(m_bridge, this);

    MamaMsg* msg = new MamaMsg();
    msg->create();
    msg->addU8("", MamaFieldMsgType.mFid, MAMA_MSG_TYPE_INITIAL);
    msg->addU8("", MamaFieldMsgStatus.mFid, MAMA_MSG_STATUS_OK);
    msg->addString("", 11, "TEST STRING");    // MdFeedName

    for (int i = 0; i < numPublishes; ++i)
    {
        publisher->send(msg);
    }
    sleep (1);

    // Destroy the publisher
    publisher->destroy();

    delete publisher;
    delete msg;

    Mama::stop(m_bridge);
}

