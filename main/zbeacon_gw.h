/*
*******************************************************************************

    zbeacon_gw.h

    Programmer:		Hetao

    Remark:

*******************************************************************************
*/

#ifndef	_MACRO_ZBEACON_GW_H
#define	_MACRO_ZBEACON_GW_H

#include <stdio.h>
#include <string>
#include <memory>
#include <functional>

#include <mqtt_client.h>

namespace zbeacon
{

/**
 * @brief format_string
 */

template< size_t capacity > class format_string
{
public:

	explicit format_string( const char * format, ... ) noexcept
	{
		va_list arglst;

		va_start( arglst, format );

		mStr[ 0 ] = mStr[ capacity - 1 ] = '\0';

		vsnprintf( mStr, capacity - 1, format, arglst );

		va_end( arglst );
	}

	char * c_str() noexcept
	{
		return mStr;
	}

	const char * c_str() const noexcept
	{
		return mStr;
	}

	size_t length() const noexcept
	{
		return strlen( mStr );
	}

	size_t size() const noexcept
	{
		return strlen( mStr );
	}

	operator char * () noexcept
	{
		return mStr;
	}

	operator const char * () const noexcept
	{
		return mStr;
	}

private:

    char mStr[ capacity ];

    format_string( const format_string & ) = delete;

    format_string & operator = ( const format_string & ) = delete;
};

/**
 * @brief MqttClient
 */

class MqttClient
{
public:

    MqttClient();

    ~MqttClient();

public:

    void Close();

    bool Connect( const esp_mqtt_client_config_t & config );

    bool Connect(
        const char *    broker_addr,
        uint16_t        broker_port,
        const char *    clientid     = nullptr,
        const char *    username     = nullptr,
        const char *    password     = nullptr,
        const char *    will_topic   = nullptr,
        const char *    will_payload = nullptr,
        int             will_pldsize = 0,
        int             will_qos     = 0,
        int             will_retain  = 0
    );

    bool Disconnect();

    bool IsConnected() const;

    int Publish( const char * topic, const char * data, int size, int qos = 0, int retain = 0 );

    int SetConfig( const esp_mqtt_client_config_t & config );

    int Subscribe( const char * topic, int qos = 0 );

    int Unsubscribe( const char * topic );

public:

    void OnBeforeConnect( const std::function< void() > & callback );

    void OnConnected( const std::function< void() > & callback );

    void OnDisconnected( const std::function< void() > & callback );

    void OnError( const std::function< void() > & callback );

    void OnMessage( const std::function< void( const esp_mqtt_event_t & ) > & callback );

protected:

    virtual void on_event_handler( esp_event_base_t base, int32_t id, void * data );

    virtual void on_event_error();

    virtual void on_event_connected();

    virtual void on_event_disconnected();

    virtual void on_event_subscribed( int msgid, esp_mqtt_error_codes_t * code, char * data, int size );

    virtual void on_event_unsubscribed( int msgid );

    virtual void on_event_published( int msgid );

    virtual void on_event_data( const esp_mqtt_event_t & event );

    virtual void on_event_before_connect();

private:

    static void esp_event_handler_routine( void * args, esp_event_base_t base, int32_t id, void * data );

private:

    esp_mqtt_client_handle_t mContext;
    EventGroupHandle_t       mNetStat;

    std::function< void() > mEvtCbError;
    std::function< void() > mEvtCbConnected;
    std::function< void() > mEvtCbDisconnected;
    std::function< void() > mEvtCbBeforeConnect;

    std::function< void( int ) > mEvtCbPublished;
    std::function< void( int ) > mEvtCbUnsubscribed;
    
    std::function< void( const esp_mqtt_event_t & ) > mEvtCbData;

    std::function< void( int, esp_mqtt_error_codes_t *, char *, int ) > mEvtCbSubscribed;

    MqttClient( const MqttClient & ) = delete;

    MqttClient & operator = ( const MqttClient & ) = delete;
};

/**
 * @brief MCPServer
 */

class MCPServer
{
public:

    typedef std::map< std::string, std::string > device_intros_table_t;
    typedef std::map< std::string, std::string > device_schema_table_t;
    typedef std::map< std::string, std::string > device_status_table_t;

    static MCPServer & GetInstance()
    {
        static MCPServer instance;

        return instance;
    }

    const std::string & GetMqttClientID() const;

    std::string GetDevices() const;

    std::string GetSchemas( const char * model ) const;

    std::string GetStatus( const char * uuid ) const;

    bool Invoke( const char * cmnd ) const;

    bool Publish( const std::string & topic, const std::string & payload, int qos = 0, bool retain = false ) const;

    bool Start();

private:

    void on_mqtt_message( const std::string & topic, const std::string & payload, bool retain );

private:

    SemaphoreHandle_t             mMutex;
    std::string                   mMqttCid;
    std::unique_ptr< MqttClient > mMqttCtx;
    intptr_t                      mMqttErr;
    std::string                   mPayload;
    std::string                   mDevices;
    device_intros_table_t         mIntross;
    device_schema_table_t         mSchemas;
    device_status_table_t         mDevStat;

    MCPServer();

    ~MCPServer();

    MCPServer( const MCPServer & ) = delete;

    MCPServer & operator = ( const MCPServer & ) = delete;
};

}

#endif	/* _MACRO_ZBEACON_GW_H */
