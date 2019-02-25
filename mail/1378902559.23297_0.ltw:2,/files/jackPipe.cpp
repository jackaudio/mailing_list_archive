#include <iostream>
#include <sstream>
#include <vector>
#include <stdexcept>

#include <cmath>

#include <jack/jack.h>
#include <pthread.h>



class IDriver
{
public:
	virtual std::size_t numChannels() const = 0;
	virtual std::size_t numFrames() const = 0;
	virtual void start() = 0;
	virtual void transfer(float ** input,
	                      float ** output) = 0;
};

class JackDriver : public IDriver
{
public:
	/** C-Tor */
	JackDriver(std::size_t numPorts) :
		client(0),
		numPorts(numPorts),
		bufferSize(0)
	{
		client = jack_client_open("jackPipe",
		                          JackNoStartServer,
		                          0);
		if(!client)
		{
			throw std::runtime_error("Cannot open JACK client");
		}
		
		/* let JACK know, we are using the callback free API */
		jack_set_process_thread(client, &JackDriver::threads, 0);
		
		/* set JACK shutdown handler */
		jack_on_shutdown(client, &JackDriver::shutdown, this);
		
		/* get buffer size */
		bufferSize = jack_get_buffer_size(client);
	}
	
	/** D-Tor */
	virtual ~JackDriver()
	{
		if(client)
		{
			jack_client_close(client);
		}
	}
	
	/* virtual */
	std::size_t numChannels() const
	{
		return numPorts;
	}
	
	/* virtual */
	std::size_t numFrames() const
	{
		return bufferSize;
	}
	
	/* virtual */
	void start()
	{
		/* activate client */
		if(jack_activate(client))
		{
			throw std::runtime_error("Cannot activate JACK client");
		}
		
		/* register ports */
		register_ports();
	}
	
	/* virtual */
	void transfer(float ** input,
	              float ** output)
	{
		/* throw if requested */
		if(!throwMessage.empty())
		{
			throw std::runtime_error(throwMessage);
		}
		
		/* wait for cycle */
		jack_nframes_t nframes = jack_cycle_wait(client);
		if(nframes == 0)
		{
			throw std::runtime_error("zero frames...");
		}
		if(nframes != bufferSize)
		{
			throw std::runtime_error("invalid frame number...");
		}
		
		/* copy input and output buffers for all ports */
		jack_default_audio_sample_t *jBuffer;
		for(std::size_t c = 0; c < numPorts; ++c)
		{
			/* copy input frames */
			jBuffer = (jack_default_audio_sample_t *) jack_port_get_buffer(inputPorts[c],
			                                                               bufferSize);
			std::copy(jBuffer, jBuffer + bufferSize, input[c]);
			
			/* copy output frames */
			jBuffer = (jack_default_audio_sample_t *) jack_port_get_buffer(outputPorts[c],
			                                                               bufferSize);
			std::copy(output[c], output[c] + bufferSize, jBuffer);
		}
		
		/* signal clients */
		jack_cycle_signal(client, 0);
	}
	
private:
	void register_ports()
	{
		jack_port_t * port;
		
		for(std::size_t c = 0; c < numPorts; ++c)
		{
			/* input ports */
			std::ostringstream iname;
			iname << "input_" << c;
			
			port = jack_port_register(client,
			                          iname.str().c_str(),
			                          JACK_DEFAULT_AUDIO_TYPE,
			                          JackPortIsInput,
			                          0);
			inputPorts.push_back(port);
			
			/* output ports */
			std::ostringstream oname;
			oname << "output_" << c;
			
			std::cout << "registering port: " << iname.str() << std::endl;
			port = jack_port_register(client,
			                          oname.str().c_str(),
			                          JACK_DEFAULT_AUDIO_TYPE,
			                          JackPortIsOutput,
			                          0);
			outputPorts.push_back(port);
		}
	}
	
	static void * threads(void *)
	{
		std::cout << "JackDriver::Threads called" << std::endl;
		return 0;
	}
	
	static void shutdown(void * arg)
	{
		std::cout << "JackDriver::shutdown called" << std::endl;
		JackDriver * drv = static_cast<JackDriver*>(arg);
		drv->throwNow("server shutdown!");
	}
	
	void throwNow(const std::string & str)
	{
		throwMessage = str;
	}
	
	jack_client_t * client;
	std::string throwMessage;
	std::size_t numPorts;
	std::size_t bufferSize;
	std::vector<jack_port_t*> inputPorts;
	std::vector<jack_port_t*> outputPorts;
	
};


class Cycler
{
public:
	Cycler(IDriver & driver) :
		driver(driver),
		numChannels(driver.numChannels()),
		numFrames(driver.numFrames())
	{
		input.resize(numChannels);
		inputPtr.resize(numChannels);
		output.resize(numChannels);
		outputPtr.resize(numChannels);
		
		for(std::size_t c = 0; c < numChannels; ++c)
		{
			input[c].resize(numFrames, .0f);
			inputPtr[c] = &input[c][0];
			output[c].resize(numFrames, .0f);
			outputPtr[c] = &output[c][0];
		}
	}
	
	void cycle()
	{
		driver.start();
		
		for(;;)
		{
			/* first get fresh data and output the processed samples */
			try
			{
				driver.transfer(&inputPtr[0],
				                &outputPtr[0]);
			}catch(const std::exception & e)
			{
				std::cout << e.what() << std::endl;
				break;
			}
			
			/*
			 * now do some work: this would normally be done in multiple
			 * threads, but for the sake of simplicity, this was left out.
			 */
			for(std::size_t c = 0; c < numChannels; ++c)
			{
				for(std::size_t f = 0; f < numFrames; ++f)
				{
					/* 
					 * Apply some sigmoidal function to all samples, just to
					 * simulate some real tough stuff...
					 */
					output[c][f] = 2.0 / (1.0 + powf(2.71282f, -10.0 * input[c][f])) - 1.0;
				}
			}
		}
		
		std::cout << "cycling finished" << std::endl;
	}
	
private:
	IDriver & driver;
	
	std::size_t numChannels;
	std::size_t numFrames;
	
	std::vector<std::vector<float> > input;
	std::vector<float *> inputPtr;
	std::vector<std::vector<float> > output;
	std::vector<float *> outputPtr;
};

void * myRTThread(void * arg)
{
	Cycler * cycler = static_cast<Cycler *>(arg);
	try
	{
		/* cycler execution */
		cycler->cycle();
	}catch(const std::exception & e)
	{
		/* lazy error handling */
		std::cout << e.what() << std::endl;
	}
	
	return 0;
}

int main()
{
	try
	{
		JackDriver driver(2);
		Cycler cycler(driver);
		
		pthread_t worker = 0;
		pthread_create(&worker, 0, &myRTThread, &cycler);
		pthread_join(worker, 0);
	}catch(const std::exception & e)
	{
		std::cout << "exception caught: " << e.what() << std::endl;
		return 1;
	}
	
	return 0;
}




