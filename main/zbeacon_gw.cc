/*
*******************************************************************************

    zbeacon_gw.cc

    Programmer:		Hetao

    Remark:

*******************************************************************************
*/

#include <cJSON.h>
#include <esp_log.h>
#include <esp_mac.h>

#include "board.h"
#include "settings.h"

#include "zbeacon_gw.h"
#include "application.h"

#define TAG "zbeacon"

#ifndef ZBEACON_CONFSRV_HOST
#define ZBEACON_CONFSRV_HOST "47.96.174.203"
#endif

#ifndef ZBEACON_CONFSRV_PORT
#define ZBEACON_CONFSRV_PORT 1883
#endif

#ifndef ZBEACON_CONFSRV_USER
#define ZBEACON_CONFSRV_USER "xiaozhi"
#endif

#ifndef ZBEACON_CONFSRV_PSWD
#define ZBEACON_CONFSRV_PSWD "564ad1a02fbf32f35198875e1bd6c3e7"
#endif

namespace zbeacon
{

// MqttClient

MqttClient::MqttClient(): mContext( nullptr )
{
    mNetStat = xEventGroupCreate();
}

MqttClient::~MqttClient()
{
    esp_mqtt_client_destroy( mContext );

    vEventGroupDelete( mNetStat );
}

void MqttClient::Close()
{
    esp_mqtt_client_destroy( mContext );

    mContext = nullptr;
}

bool MqttClient::Connect( const esp_mqtt_client_config_t & config )
{
    Close();

    mContext = esp_mqtt_client_init( &config );

    if ( ! mContext )
    {
        return false;
    }

    if ( ESP_OK != esp_mqtt_client_register_event( mContext, MQTT_EVENT_ANY, esp_event_handler_routine, this ) )
    {
        return false;
    }

    return ESP_OK == esp_mqtt_client_start( mContext );
}

bool MqttClient::Connect(
    const char *    broker_addr,
    uint16_t        broker_port,
    const char *    clientid,
    const char *    username,
    const char *    password,
    const char *    will_topic,
    const char *    will_payload,
    int             will_pldsize,
    int             will_qos,
    int             will_retain
)
{
    esp_mqtt_client_config_t config = {};

    config.broker.address.hostname             = broker_addr;
    config.broker.address.port                 = broker_port;
    config.broker.address.transport            = MQTT_TRANSPORT_OVER_TCP;
    config.credentials.client_id               = clientid;
    config.credentials.username                = username;
    config.credentials.authentication.password = password;
    config.session.last_will.topic             = will_topic;
    config.session.last_will.msg               = will_payload;
    config.session.last_will.msg_len           = will_pldsize;
    config.session.last_will.qos               = will_qos;
    config.session.last_will.retain            = will_retain;
    config.session.keepalive                   = 120;

    if ( nullptr == clientid )
    {
        config.credentials.set_null_client_id = true;
    }
    else if ( 0 == strcmp( clientid, "" ) )
    {
        config.credentials.client_id = nullptr;

        config.credentials.set_null_client_id = false;
    }

    return Connect( config );
}

bool MqttClient::Disconnect()
{
    return ESP_OK == esp_mqtt_client_disconnect( mContext );
}

bool MqttClient::IsConnected() const
{
    return BIT0 == ( xEventGroupGetBits( mNetStat ) & BIT0 );
}

int MqttClient::Publish( const char * topic, const char * data, int size, int qos, int retain )
{
    return esp_mqtt_client_publish( mContext, topic, data, size, qos, retain );
}

int MqttClient::SetConfig( const esp_mqtt_client_config_t & config )
{
    return esp_mqtt_set_config( mContext, &config );
}

int MqttClient::Subscribe( const char * topic, int qos )
{
    return esp_mqtt_client_subscribe_single( mContext, topic, qos );
}

int MqttClient::Unsubscribe( const char * topic )
{
    return esp_mqtt_client_unsubscribe( mContext, topic );
}

void MqttClient::OnBeforeConnect( const std::function< void() > & callback )
{
    mEvtCbBeforeConnect = callback;
}

void MqttClient::OnConnected( const std::function< void() > & callback )
{
    mEvtCbConnected = callback;
}

void MqttClient::OnDisconnected( const std::function< void() > & callback )
{
    mEvtCbDisconnected = callback;
}

void MqttClient::OnError( const std::function< void() > & callback )
{
    mEvtCbError = callback;
}

void MqttClient::OnMessage( const std::function< void( const esp_mqtt_event_t & ) > & callback )
{
    mEvtCbData = callback;
}

void MqttClient::on_event_handler( esp_event_base_t base, int32_t id, void * data )
{
    auto event = static_cast< esp_mqtt_event_t * >( data );

    switch ( id )
    {
    case MQTT_EVENT_ERROR:

        xEventGroupClearBits( mNetStat, 0x00FFFFFF );

        xEventGroupSetBits( mNetStat, BIT3 );

        on_event_error();

        break;

    case MQTT_EVENT_CONNECTED:

        xEventGroupClearBits( mNetStat, 0x00FFFFFF );

        xEventGroupSetBits( mNetStat, BIT0 );

        on_event_connected();

        break;

    case MQTT_EVENT_DISCONNECTED:

        xEventGroupClearBits( mNetStat, 0x00FFFFFF );

        xEventGroupSetBits( mNetStat, BIT1 );

        on_event_disconnected();

        break;

    case MQTT_EVENT_SUBSCRIBED:

        on_event_subscribed( event->msg_id, event->error_handle, event->data, event->data_len );

        break;

    case MQTT_EVENT_UNSUBSCRIBED:

        on_event_unsubscribed( event->msg_id );

        break;

    case MQTT_EVENT_PUBLISHED:

        on_event_published( event->msg_id );

        break;

    case MQTT_EVENT_DATA:

        on_event_data( *event );

        break;

    case MQTT_EVENT_BEFORE_CONNECT:

        on_event_before_connect();

        break;

    default: break;
    }
}

void MqttClient::on_event_error()
{
    if ( mEvtCbError )
    {
        mEvtCbError();
    }
}

void MqttClient::on_event_connected()
{
    if ( mEvtCbConnected )
    {
        mEvtCbConnected();
    }
}

void MqttClient::on_event_disconnected()
{
    if ( mEvtCbDisconnected )
    {
        mEvtCbDisconnected();
    }
}

void MqttClient::on_event_subscribed( int msgid, esp_mqtt_error_codes_t * code, char * data, int size )
{
    if ( mEvtCbSubscribed )
    {
        mEvtCbSubscribed( msgid, code, data, size );
    }
}

void MqttClient::on_event_unsubscribed( int msgid )
{
    if ( mEvtCbUnsubscribed )
    {
        mEvtCbUnsubscribed( msgid );
    }
}

void MqttClient::on_event_published( int msgid )
{
    if ( mEvtCbPublished )
    {
        mEvtCbPublished( msgid );
    }
}

void MqttClient::on_event_data( const esp_mqtt_event_t & event )
{
    if ( mEvtCbData )
    {
        mEvtCbData( event );
    }
}

void MqttClient::on_event_before_connect()
{
    if ( mEvtCbBeforeConnect )
    {
        mEvtCbBeforeConnect();
    }
}

void MqttClient::esp_event_handler_routine( void * args, esp_event_base_t base, int32_t id, void * data )
{
    static_cast< MqttClient * >( args )->on_event_handler( base, id, data );
}

// MCPServer

MCPServer::MCPServer(): mMqttCtx( new MqttClient ), mMqttErr( 0 )
{
    mMutex = xSemaphoreCreateMutex();

    {
        uint8_t mac[ 6 ];

        if ( ESP_OK == esp_efuse_mac_get_default( mac ) )
        {
            mMqttCid = format_string< 16 >( "%02X%02X%02X%02X%02X%02X",

                mac[ 0 ], mac[ 1 ], mac[ 2 ],
                mac[ 3 ], mac[ 4 ], mac[ 5 ]
            );
        }
    }

    Settings settings( "zbeacon", true );

    if ( settings.GetString( "endpoint" ) == "" )
    {
        format_string< 128 > endpoint( "%s:%d", ZBEACON_CONFSRV_HOST, ZBEACON_CONFSRV_PORT );

        settings.SetString( "endpoint", endpoint.c_str()     );
        settings.SetString( "username", ZBEACON_CONFSRV_USER );
        settings.SetString( "password", ZBEACON_CONFSRV_PSWD );
    }
}

MCPServer::~MCPServer()
{
    vSemaphoreDelete( mMutex );
}

const std::string & MCPServer::GetMqttClientID() const
{
    return mMqttCid;
}

std::string MCPServer::GetDevices() const
{
    std::string result;

    if ( mMqttCtx && mMqttCtx->IsConnected() && pdTRUE == xSemaphoreTake( mMutex, portMAX_DELAY ) )
    {
        result = mDevices;

        if ( mIntross.empty() )
        {
            Publish( format_string< 128 >( "xiaozhi/%s/send/sync", mMqttCid.c_str() ).c_str(), "{\"scope\":\"devices\"}" );
        }

        xSemaphoreGive( mMutex );
    }

    return result;
}

std::string MCPServer::GetSchemas( const char * model ) const
{
    std::string result;

    if ( mMqttCtx && mMqttCtx->IsConnected() && pdTRUE == xSemaphoreTake( mMutex, portMAX_DELAY ) )
    {
        auto first = mSchemas.find( model );

        if ( mSchemas.end() != first )
        {
            result = first->second;
        }
        else
        {
            Publish( format_string< 128 >( "xiaozhi/%s/send/sync", mMqttCid.c_str() ).c_str(), "{\"scope\":\"schema\"}" );
        }

        xSemaphoreGive( mMutex );
    }

    return result;
}

std::string MCPServer::GetStatus( const char * uuid ) const
{
    std::string result;

    if ( mMqttCtx && mMqttCtx->IsConnected() && pdTRUE == xSemaphoreTake( mMutex, portMAX_DELAY ) )
    {
        auto first = mDevStat.find( uuid );

        if ( mDevStat.end() != first )
        {
            result = first->second;
        }

        xSemaphoreGive( mMutex );
    }

    return result;
}

bool MCPServer::Invoke( const char * cmnd ) const
{
    if ( ( ! mMqttCtx ) || ( ! mMqttCtx->IsConnected() ) )
    {
        ESP_LOGE( TAG, "The MQTT endpoint has been disconnected" );

        return false;
    }

    format_string< 128 > topic( "xiaozhi/%s/send/cmnd", mMqttCid.c_str() );

    return 0 == mMqttCtx->Publish( topic, cmnd, cmnd ? strlen( cmnd ) : 0, 0 );
}

bool MCPServer::Publish( const std::string & topic, const std::string & payload, int qos, bool retain ) const
{
    if ( ( ! mMqttCtx ) || ( ! mMqttCtx->IsConnected() ) )
    {
        ESP_LOGE( TAG, "The MQTT endpoint has been disconnected" );

        return false;
    }

    return mMqttCtx->Publish( topic.c_str(), payload.c_str(), static_cast< int >( payload.size() ), qos, retain ? 1 : 0 ) >= 0;
}

bool MCPServer::Start()
{
    Settings settings( "zbeacon", false );

    auto endpoint = settings.GetString( "endpoint" );
    auto clientid = settings.GetString( "clientid", mMqttCid );
    auto username = settings.GetString( "username" );
    auto password = settings.GetString( "password" );

    if ( endpoint.empty() )
    {
        ESP_LOGW( TAG, "MQTT endpoint is not specified" );

        return false;
    }

    std::string broker_address;

    int broker_port = 8883;

    size_t pos = endpoint.find( ':' );

    if ( pos != std::string::npos )
    {
        broker_address = endpoint.substr( 0, pos );

        broker_port = std::stoi( endpoint.substr( pos + 1 ) );
    }
    else
    {
        broker_address = endpoint;
    }

    if ( ! mMqttCtx->Connect(
        broker_address.c_str(),
        broker_port,
        clientid.c_str(),
        username.c_str(),
        password.c_str(),
        format_string< 128 >( "xiaozhi/%s/LWT", mMqttCid.c_str() ).c_str(),
        "offline",
        7
    ) )
    {
        ESP_LOGE( TAG, "Failed to connect to endpoint" );

        return false;
    }

    mMqttCtx->OnBeforeConnect( [ this ](){

        if ( mMqttErr > 4 )
        {
            esp_mqtt_client_config_t config = {};

            config.broker.address.hostname             = ZBEACON_CONFSRV_HOST;
            config.broker.address.port                 = ZBEACON_CONFSRV_PORT;
            config.broker.address.transport            = MQTT_TRANSPORT_OVER_TCP;
            config.credentials.username                = ZBEACON_CONFSRV_USER;
            config.credentials.authentication.password = ZBEACON_CONFSRV_PSWD;

            if ( ESP_OK == mMqttCtx->SetConfig( config ) )
            {
                mMqttErr = -1;

                ESP_LOGW( TAG, "Switch to the configuration server" );
            }
        }
    } );

    mMqttCtx->OnConnected( [ this ](){

        typedef format_string< 128 > topic_t;

        ESP_LOGI( TAG, "Connected to endpoint" );

        mMqttErr = 0;

        mMqttCtx->Subscribe( topic_t( "xiaozhi/%s/recv/+", mMqttCid.c_str() ) );

        mMqttCtx->Publish( topic_t( "xiaozhi/%s/LWT", mMqttCid.c_str() ), "online", 6 );
    } );

    mMqttCtx->OnDisconnected( [ this ](){

        ESP_LOGW( TAG, "Disconnected from endpoint" );
    } );

    mMqttCtx->OnError( [ this ](){

        if ( mMqttErr >= 0 )
        {
            mMqttErr++;
        }

        ESP_LOGE( TAG, "Error from endpoint" );
    } );

    mMqttCtx->OnMessage( [ this ]( const esp_mqtt_event_t & event ){

        auto topic = std::string( event.topic, event.topic_len );

        auto payload = std::string( event.data, event.data_len );

        if ( event.data_len == event.total_data_len )
        {
            on_mqtt_message( topic, payload, event.retain );
        }
        else
        {
            mPayload.append( payload );

            if ( mPayload.size() >= event.total_data_len )
            {
                on_mqtt_message( topic, mPayload, event.retain );

                mPayload.clear();
            }
        }
    } );

    ESP_LOGW( TAG, "Connecting to endpoint %s", endpoint.c_str() );

    return true;
}

void MCPServer::on_mqtt_message( const std::string & topic, const std::string & payload, bool retain )
{
    std::string::size_type pos = topic.find_last_of( '/' );

    if ( std::string::npos == pos )
    {
        return;
    }

    std::string name = topic.substr( pos + 1 );

    if ( name == "devices" )
    {
        cJSON * root = cJSON_Parse( payload.c_str() );

        if ( root && cJSON_IsObject( root ) && pdTRUE == xSemaphoreTake( mMutex, portMAX_DELAY ) )
        {
            cJSON * item;

            cJSON_ArrayForEach( item, root )
            {
                if ( cJSON_IsString( item ) )
                {
                    ESP_LOGI( TAG, "Add device intros (%s)", item->string );

                    mIntross[ std::string( item->string ) ] = std::string( item->valuestring );
                }
                else if ( cJSON_IsNull( item ) )
                {
                    ESP_LOGW( TAG, "Del device intros (%s)", item->string );

                    mIntross.erase( std::string( item->string ) );
                }
                else
                {
                    ESP_LOGE( TAG, "Update device intros failed! String expected (%s)", item->string );
                }
            }

            mDevices = "[";

            size_t count = 0;

            for ( auto & value: mIntross )
            {
                if ( count > 0 )
                {
                    mDevices.append( "," );
                }

                mDevices.append( value.second.c_str() );

                ++count;
            }

            mDevices.append( "]" );

            xSemaphoreGive( mMutex );
        }

        cJSON_Delete( root );
    }
    else if ( name == "schemas" )
    {
        cJSON * root = cJSON_Parse( payload.c_str() );

        if ( root && cJSON_IsObject( root ) && pdTRUE == xSemaphoreTake( mMutex, portMAX_DELAY ) )
        {
            cJSON * item;

            cJSON_ArrayForEach( item, root )
            {
                if ( cJSON_IsString( item ) )
                {
                    ESP_LOGI( TAG, "Add device schema (%s)", item->string );

                    mSchemas[ std::string( item->string ) ] = std::string( item->valuestring );
                }
                else if ( cJSON_IsNull( item ) )
                {
                    ESP_LOGW( TAG, "Del device schema (%s)", item->string );

                    mSchemas.erase( std::string( item->string ) );
                }
                else
                {
                    ESP_LOGE( TAG, "Update device schema failed! String expected (%s)", item->string );
                }
            }

            xSemaphoreGive( mMutex );
        }

        cJSON_Delete( root );
    }
    else if ( name == "status" )
    {
        cJSON * root = cJSON_Parse( payload.c_str() );

        if ( root && cJSON_IsObject( root ) && pdTRUE == xSemaphoreTake( mMutex, portMAX_DELAY ) )
        {
            cJSON * item;

            cJSON_ArrayForEach( item, root )
            {
                if ( cJSON_IsString( item ) )
                {
                    ESP_LOGI( TAG, "Update device status (%s)", item->string );

                    mDevStat[ std::string( item->string ) ] = std::string( item->valuestring );
                }
                else if ( cJSON_IsNull( item ) )
                {
                    ESP_LOGW( TAG, "Remove device status (%s)", item->string );

                    mDevStat.erase( std::string( item->string ) );
                }
                else
                {
                    ESP_LOGE( TAG, "Update device status failed! String expected (%s)", item->string );
                }
            }

            xSemaphoreGive( mMutex );
        }

        cJSON_Delete( root );
    }
    else if ( name == "ping" )
    {
        Publish( format_string< 128 >( "xiaozhi/%s/send/reply", mMqttCid.c_str() ).c_str(), "{\"ping\":\"Done\"}" );
    }
    else if ( name == "reboot" )
    {
        Application::GetInstance().Reboot();
    }
    else if ( name == "config" )
    {
        cJSON * root = cJSON_Parse( payload.c_str() );

        if ( root && cJSON_IsObject( root ) )
        {
            Settings settings( "zbeacon", true );

            cJSON * item;

            cJSON_ArrayForEach( item, root )
            {
                if ( cJSON_IsString( item ) )
                {
                    if ( settings.GetString( item->string ) != item->valuestring )
                    {
                        settings.SetString( item->string, item->valuestring );
                    }
                }
                else if ( cJSON_IsNumber( item ) )
                {
                    if ( settings.GetInt( item->string ) != item->valueint )
                    {
                        settings.SetInt( item->string, item->valueint );
                    }
                }
            }
        }

        cJSON_Delete( root );

        Publish( format_string< 128 >( "xiaozhi/%s/send/reply", mMqttCid.c_str() ).c_str(), "{\"config\":\"Done\"}" );
    }
}

}
