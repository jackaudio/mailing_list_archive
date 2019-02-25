/**
 * Royi Freifeld 2011
 */
#ifndef JACKSERVERMANAGER_HPP
#define JACKSERVERMANAGER_HPP

#include <jack/control.h>
#include <alsa/asoundlib.h>

#include <stdexcept>

#define AUDIO_DEVICE_MAX_NUMBER 2

class JackServerManager
{
    private:

        jackctl_server_t* _server;      

        const JSList* _server_params;   

        jackctl_driver_t* _alsa_driver; 

        const JSList* _driver_params;   

        /**
          * Checks whether the playback audio device is not busy and can be
          *     accessed by the ALSA driver
          *
          * \param audio_device_num - Playback audio device number
          *
          * \return True if can connect to the device, false otherwise
          */
        bool check_device_availability(int audio_device_num);

        /**
          * \brief Retrieves the ALSA driver
          *
          * \pre _server != NULL
          *
          * \throw runtime_error if it was impossible to retrieve ALSA driver
          * \return a pointer to the alsa driver or NULL if couldn't retrieve it
          */
        jackctl_driver_t* get_alsa_driver()
            throw(std::runtime_error);

        /**
          * \brief Initializes the parameters of the server and the driver
          *             to be default values:
          * <ul>
          * 	<li>Real-Time mode      [default:  yes]
          * 	<li>Sample rate         [default: 44100]
          * 	<li>Number of channels  [default: 2]
          * </ul>
          *
          * \pre _server_params != NULL, _driver_params != NULL
          *
          * \param audio_device_num - Playback audio device number
          *
          * \throw runtime_error if it was impossible to set parameter
          */
        void set_server_parameters(int audio_device_num)
            throw(std::runtime_error);

        /**
          * \brief Initializes the JACK server.
          *
          * \param audio_device_num The audio device number to initialize the server with
          *
          * \throw runtime_error in case that an error has occured
          */
        void init(int audio_device_num)
            throw(std::runtime_error);

    public:

        /**
          * \brief Constructor. Initializes all data associated with this class and starts Jack server
          *
          * \throw runtime_error in case that the server could not be initialized or started
          */
        JackServerManager()
            throw(std::runtime_error);

        /**
          * \brief Destructor. Deletes all memory associated with this class and closes all
          *                     open connection to actual Jack server
          */
        virtual ~JackServerManager();

        /**
          * \brief Starts the JACK server
          *
          * \pre _server != NULL, _alsa_driver != NULL
          *
          * \throw runtime_error in case server could not be started
          */
        void start_server()
            throw(std::runtime_error);

        /**
          * \brief Stops the JACK server
          *
          * \pre _server != NULL
          *
          * \throw runtime_errir in case server could not be stopped
          */
        void stop_server()
            throw(std::runtime_error);
};

#endif // JACKSERVERMANAGER_HPP
