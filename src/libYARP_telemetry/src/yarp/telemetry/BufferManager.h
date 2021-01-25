/*
 * Copyright (C) 2006-2021 Istituto Italiano di Tecnologia (IIT)
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the
 * BSD-3-Clause license. See the accompanying LICENSE file for details.
 */

#ifndef YARP_TELEMETRY_BUFFER_MANAGER_H
#define YARP_TELEMETRY_BUFFER_MANAGER_H

#include <yarp/telemetry/Buffer.h>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <assert.h>
#include <yarp/os/Time.h>
#include <matioCpp/matioCpp.h>

namespace yarp::telemetry {

using dimensions_t = std::vector<size_t>;

struct BufferInfo {
    std::string m_var_name;
    dimensions_t m_dimensions{ 1,1 };
};



template<class T>
class BufferManager {

public:
    BufferManager() = delete;
    BufferManager(const std::string& filename, const std::vector<BufferInfo>& listOfVars,
                  size_t n_samples, bool auto_save=false) : m_filename(filename), m_auto_save(auto_save) {
        assert(listOfVars.size() != 0);
        assert(!filename.empty());
        for (const auto& s : listOfVars) {
            m_buffer_map.insert(std::pair<std::string, yarp::telemetry::Buffer<T>>(s.m_var_name,Buffer<T>(n_samples)));
            m_dimensions_map.insert(std::pair<std::string, yarp::telemetry::dimensions_t>(s.m_var_name, s.m_dimensions));
		}
	}

    ~BufferManager() {
        if (m_auto_save) {
            saveToFile();
        }
    }

    inline void push_back(const std::vector<T>& elem, const std::string& var_name)
    {
        assert(elem.size() == m_dimensions_map.at(var_name)[0] * m_dimensions_map.at(var_name)[1]);
        m_buffer_map.at(var_name).push_back(Record<T>(yarp::os::Time::now(), elem));
    }


    inline void push_back(std::vector<T>&& elem, const std::string& var_name)
    {
        assert(elem.size() == m_dimensions_map.at(var_name)[0] * m_dimensions_map.at(var_name)[1]);
        m_buffer_map.at(var_name).push_back(Record<T>(yarp::os::Time::now(), std::move(elem)));

    }

    bool saveToFile() {

        // now we initialize the proto-timeseries structure
        std::vector<matioCpp::Variable> signalsVect;
        // and the matioCpp struct for these signals
        for (auto& [var_name, buff] : m_buffer_map) {
            if (!buff.full())
            {
                std::cout << "not enough data points collected for " << var_name << std::endl;
                continue;
            }

            std::vector<T> linear_matrix;
            std::vector<double> timestamp_vector;

            // the number of timesteps is the size of our collection
            int num_timesteps = buff.size();


            // we first collapse the matrix of data into a single vector, in preparation for matioCpp convertion
            // TODO put mutexes here....
            for (auto& _cell : buff)
            {
                for (auto& _el : _cell.m_datum)
                {
                    linear_matrix.push_back(_el);
                }
                timestamp_vector.push_back(_cell.m_ts);
            }
            buff.clear();

            // now we start the matioCpp convertion process

            // first create timestamps vector
            matioCpp::Vector<double> timestamps("timestamps");
            timestamps = timestamp_vector;

            // and the structures for the actual data too
            std::vector<matioCpp::Variable> test_data;

            // now we create the vector for the dimensions

            std::vector<int> dimensions_data_vect {num_timesteps, (int)m_dimensions_map.at(var_name)[0] , (int)m_dimensions_map.at(var_name)[1] };
            matioCpp::Vector<int> dimensions_data("dimensions");
            dimensions_data = dimensions_data_vect;

            // now we populate the matioCpp matrix
            matioCpp::MultiDimensionalArray<T> out("data", {m_dimensions_map.at(var_name)[0] , m_dimensions_map.at(var_name)[1], (size_t)num_timesteps }, linear_matrix.data());
            test_data.emplace_back(out); // Data

            test_data.emplace_back(dimensions_data); // dimensions vector

            test_data.emplace_back(matioCpp::String("name", var_name)); // name of the signal
            test_data.emplace_back(timestamps);

            // we store it as a matioCpp struct
            matioCpp::Struct data_struct(var_name, test_data);

            // now we create the vector that stores different signals (in case we had more than one)
            signalsVect.emplace_back(data_struct);


        }
        auto point_pos = m_filename.find('.');
        matioCpp::Struct timeSeries(std::string(m_filename.begin(), m_filename.begin()+point_pos), signalsVect);
        // and finally we write the file
        matioCpp::File file = matioCpp::File::Create(m_filename);
        return file.write(timeSeries);
    }
private:
    std::string m_filename;
    bool m_auto_save;
    std::map<std::string, Buffer<T>> m_buffer_map;
    std::map<std::string, dimensions_t> m_dimensions_map;

};

} // yarp::telemetry

#endif