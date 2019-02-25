/**
 * Royi Freifeld 2011
 */

#include "JackServerManager.hpp"

#include <sstream>

#include <cassert>
#include <cmath>
#include <cstring>

#define SERVER_NAME_PARAM "name"
#define SERVER_NAME_VALUE "default"

#define SERVER_REALTIME_PARAM "realtime"
#define SERVER_REALTIME_VALUE true

#define DRIVER_RATE_PARAM "rate"
#define DRIVER_RATE_VALUE 44100

#define DRIVER_PERIOD_PARAM "period"
#define DRIVER_PERIOD_VALUE 512

#define ALSA_DRIVER_NAME "alsa"

#define DRIVER_OUTCHANNELS_PARAM "outchannels"
#define DRIVER_OUTCHANNELS_VALUE 2

#define DRIVER_DEVICE_PARAM "device"

using namespace std;

JackServerManager::JackServerManager()
    throw(runtime_error) :
    _server(NULL),
    _server_params(NULL),
    _alsa_driver(NULL),
    _driver_params(NULL)
{
    for (int i = AUDIO_DEVICE_MAX_NUMBER ; i >= 0 ; --i)
    {
    	try
    	{
    		this->init(i);
    		break;
    	}
    	catch (runtime_error& re)
    	{
    		//TODO LOG
    	}
    }
}

JackServerManager::~JackServerManager()
{
    if (_server != NULL)
    {
        jackctl_server_destroy(_server);
        _server = NULL;
    }
}

void JackServerManager::start_server()
    throw(runtime_error)
{
    assert(_server != NULL);
    assert(_alsa_driver != NULL);

    if (!jackctl_server_start(_server, _alsa_driver))
    {
        throw runtime_error("Could not start Jack server");
    }
}

void JackServerManager::stop_server()
    throw(runtime_error)
{
    assert(_server != NULL);

    if (!jackctl_server_stop(_server))
    {
        throw runtime_error("Could not stop Jack server");
    }
}

/**
  * \brief returns a specific parameter from the parameters list
  *
  * \param param_list - The parameters list
  * \param param_name - The parameter to return
  *
  * \return the parameter or NULL if wasn't found
  */
jackctl_parameter_t* jackctl_get_parameter(const JSList* param_list, const char* param_name)
{
    jackctl_parameter_t* cur_param = NULL;
    const char* cur_param_name = NULL;

    while (param_list != NULL)
    {
        cur_param = static_cast<jackctl_parameter_t*>(param_list->data);
        cur_param_name = jackctl_parameter_get_name(cur_param);

        if (!strcmp(cur_param_name,param_name))
        {
            return cur_param; // I hate returning in the middle of a function, but it's a must here
        }

        param_list = jack_slist_next(param_list);   // jump to next link in the chain (handles NULL)
    }

    return NULL; // The parameter wasn't found
}

bool JackServerManager::check_device_availability(int audio_device_num)
{
    //TODO Opening and closing the device might not be the best way to do
    //          check the device availability.
    //          but that should do for now
    bool to_return = true;

    snd_pcm_t* handle = NULL;

    stringstream device_name;
    device_name << "plughw:" << audio_device_num << ",0";

    int err_code = snd_pcm_open(&handle,device_name.str().c_str(),SND_PCM_STREAM_PLAYBACK,0);
    printf("%s\n",snd_strerror(err_code));
    if (err_code < 0) // Could not open PCM properly
    {
        to_return = false;
    }

    if (handle != NULL)
    {
        snd_pcm_close(handle);
        handle = NULL;
    }

    return to_return;
}

jackctl_driver_t* JackServerManager::get_alsa_driver()
    throw (runtime_error)
{
    assert(_server != NULL);

    jackctl_driver_t* to_return = NULL;

    const JSList* drivers = jackctl_server_get_drivers_list(_server); //ptr to the beginning of the list
    const JSList* drv_ptr = drivers; //running ptr

    while (drv_ptr != NULL)
    {
        to_return = static_cast<jackctl_driver_t*>(drv_ptr->data);
        const char* cur_drv_name = jackctl_driver_get_name(to_return);
        int ret_cmp = strcmp(ALSA_DRIVER_NAME,
                             cur_drv_name);
        if (!ret_cmp)
        {
            break; // Hate using break. But it's a must here
        }

        drv_ptr = drv_ptr->next;
    }

    if (drv_ptr == NULL)
    {
        throw runtime_error("Could not locate the ALSA driver");
    }

    return to_return;
}

/**
  * \brief Sets a single parameter in the parameters list
  *
  * \pre parameters != NULL
  *
  * \param parameters - The parameters list
  * \param param_name - The parameter to set
  * \param value - The value of the parameter
  * \param err_msg - Message to throw in case that action failed
  *
  * \throw runtime_error in case that it was impossible to set the parameter
  */
void set_parameter(const JSList* parameters,
                   const string& param_name,
                   jackctl_parameter_value& value,
                   const string& err_msg)
    throw (runtime_error)
{
    assert(parameters != NULL);

    jackctl_parameter_t* param = NULL;

    param = jackctl_get_parameter(parameters, param_name.c_str());
    if (param != NULL)
    {
        jackctl_parameter_set_value(param,&value);
    }
    else
    {
        throw runtime_error(err_msg);
    }
}

void JackServerManager::set_server_parameters(int audio_device_num)
    throw (runtime_error)
{
    assert(_server_params != NULL);

    jackctl_parameter_value value;

    // server name
    strcpy(value.str,SERVER_NAME_VALUE);
    set_parameter(_server_params, SERVER_NAME_PARAM, value, "Could not change server name");

    // realtime status
    value.b = SERVER_REALTIME_VALUE;
    set_parameter(_server_params, SERVER_REALTIME_PARAM, value, "Could not set realtime state");

    // driver sample rate
    value.ui = DRIVER_RATE_VALUE;
    set_parameter(_driver_params, DRIVER_RATE_PARAM, value, "Could not set driver sample rate");

    // driver period
    value.ui = DRIVER_PERIOD_VALUE;
    set_parameter(_driver_params, DRIVER_PERIOD_PARAM, value, "Could not set driver period");

    // number of output channels
    value.ui = DRIVER_OUTCHANNELS_VALUE;
    set_parameter(_driver_params, DRIVER_OUTCHANNELS_PARAM, value, "Could not set number of output channels");

    // device
    stringstream device;
    device << "hw:" << audio_device_num << ",0";

    strcpy(value.str, device.str().c_str());
    set_parameter(_driver_params, DRIVER_DEVICE_PARAM, value, "Could not use device");
}

void JackServerManager::init(int audio_device_num)
    throw(runtime_error)
{
    stringstream err_msg;
    if (!this->check_device_availability(audio_device_num))
    {
        err_msg << "Device number " << audio_device_num << " is not available";
        throw runtime_error(err_msg.str());
    }

    _server = jackctl_server_create(NULL,NULL);
    if (_server == NULL)
    {
        err_msg << "Could not create the Jack server";
        throw runtime_error(err_msg.str());
    }

    _alsa_driver = this->get_alsa_driver();
    if (_alsa_driver == NULL)
    {
        err_msg << "Could not get the ALSA driver";
        throw runtime_error(err_msg.str());
    }

    _server_params = jackctl_server_get_parameters(_server);
    _driver_params = jackctl_driver_get_parameters(_alsa_driver);

    this->set_server_parameters(audio_device_num);
}
