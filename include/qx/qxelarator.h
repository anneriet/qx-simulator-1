/**
 * @file   qxelarator.h
 * @author Imran Ashraf
 * @brief  header file for qxelarator python interface
 */

#ifndef QXELARATOR_H
#define QXELARATOR_H

#include <iostream>
#include "qx/simulator.h"


class QX
{
public:
    qx::simulator * qx_sim;

    QX()
    {
        try
        {
            qx_sim = new qx::simulator();
        }
        catch (std::exception &e)
        {
            std::cerr << "error creating qx::simulator " << std::endl;
            std::cerr << e.what() << std::endl;
        }
        // By calling QXelerator we already have access to the simulation results, so log level set to errors only (qx used to always print everything to cout)
        qx::logger::set_log_level("LOG_ERROR");
    }
    ~QX()
    {
        delete(qx_sim);
    }

    void set(std::string qasm_file_name)
    {
        qx_sim->set(qasm_file_name);
    }

    void execute(size_t navg=0)
    {
        qx_sim->execute(navg);
    }

    bool get_measurement_outcome(size_t q)
    {
        return qx_sim->move(q);
    }

    std::vector<double> get_average_measurement()
    {
        return qx_sim->get_average_measurement();
    }

    // Executes the circuit once and returns an average measurement result calculated using a binomial distribution
     std::vector<double> execute_and_get_average_measurement(size_t n)
    {
        execute();
        return qx_sim->get_average_measurement(n);
    }
    
    std::string get_state()
    {
        return qx_sim->get_state();
    }
    
    // std::vector<complex_t> get_state_vector()
    // {
    //     std::vector<xpu::complex_d, xpu::aligned_memory_allocator<xpu::complex_d, 64> > vec = qx_sim->get_state_vector();
    //     complex_t * data = (qx_sim->get_state_vector()).data();
    //     std::vector<complex_t> cvec(std::begin(data), std::end(data));
    //     return vec.data();
    // }
    
    std::vector<std::complex<double>> get_state_vector()
    {
        return qx_sim->get_state_vector();
    }

};

#endif
