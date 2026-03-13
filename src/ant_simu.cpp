#include <vector>
#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include "fractal_land.hpp"
#include "ant.hpp"
#include "pheronome.hpp"
#include "renderer.hpp"
#include "window.hpp"
#include "rand_generator.hpp"

using hr_clock = std::chrono::high_resolution_clock;
using duration_ms = std::chrono::duration<double, std::milli>;

// Variáveis globais para medição de performance 
double total_events = 0.0;
double total_ants = 0.0;
double total_evap = 0.0;
double total_update = 0.0;
double total_render = 0.0;
double total_blt = 0.0;
double total_iter = 0.0;
std::size_t nb_iters_mesurees = 0;

// Versão vetorizada do avanço das formigas
void advance_ant(
    const fractal_land& land, pheronome& phen,
    const position_t& pos_nest, const position_t& pos_food,
    std::vector<position_t>& positions,
    std::vector<ant::state>& states,
    std::vector<std::size_t>& seeds,
    std::size_t idx,
    std::size_t& cpteur )
{
    auto ant_choice = [&seeds, idx]() mutable { return rand_double( 0., 1., seeds[idx] ); };
    auto dir_choice = [&seeds, idx]() mutable { return rand_int32( 1, 4, seeds[idx] ); };
    double consumed_time = 0.;

    while ( consumed_time < 1. ) {
        int ind_pher = ( states[idx] == ant::loaded ? 1 : 0 );
        double choix = ant_choice();
        position_t old_pos = positions[idx];
        position_t new_pos = old_pos;

        double max_phen = std::max({
            phen(new_pos.x - 1, new_pos.y)[ind_pher],
            phen(new_pos.x + 1, new_pos.y)[ind_pher],
            phen(new_pos.x, new_pos.y - 1)[ind_pher],
            phen(new_pos.x, new_pos.y + 1)[ind_pher]
        });

        if ((choix > ant::get_exploration_coef()) || (max_phen <= 0.)) {
            do {
                new_pos = old_pos;
                int d = dir_choice();
                if (d == 1) new_pos.x -= 1;
                if (d == 2) new_pos.y -= 1;
                if (d == 3) new_pos.x += 1;
                if (d == 4) new_pos.y += 1;
            } while (phen[new_pos][ind_pher] == -1);
        } else {
            if (phen(new_pos.x - 1, new_pos.y)[ind_pher] == max_phen)
                new_pos.x -= 1;
            else if (phen(new_pos.x + 1, new_pos.y)[ind_pher] == max_phen)
                new_pos.x += 1;
            else if (phen(new_pos.x, new_pos.y - 1)[ind_pher] == max_phen)
                new_pos.y -= 1;
            else
                new_pos.y += 1;
        }

        consumed_time += land(new_pos.x, new_pos.y);
        phen.mark_pheronome(new_pos);
        positions[idx] = new_pos;

        if (positions[idx] == pos_nest) {
            if (states[idx] == ant::loaded) cpteur += 1;
            states[idx] = ant::unloaded;
        }
        if (positions[idx] == pos_food) states[idx] = ant::loaded;
    }
}

int main(int nargs, char* argv[])
{
    SDL_Init( SDL_INIT_VIDEO );
    std::size_t seed = 2026; 
    const int nb_ants = 5000;
    const double eps = 0.8;
    const double alpha = 0.7;
    const double beta = 0.999;
    
    position_t pos_nest{256, 256};
    position_t pos_food{500, 500};
    
    fractal_land land(8, 2, 1., 1024);
    
    // Normalização do terreno
    double max_val = 0.0, min_val = 0.0;
    for (size_t i = 0; i < land.dimensions(); ++i)
        for (size_t j = 0; j < land.dimensions(); ++j) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }
    double delta = max_val - min_val;
    for (size_t i = 0; i < land.dimensions(); ++i)
        for (size_t j = 0; j < land.dimensions(); ++j)
            land(i,j) = (land(i,j) - min_val) / delta;

    ant::set_exploration_coef(eps);

    // Inicialização Vetorizada 
    auto gen_ant_pos = [&land, &seed] () { return rand_int32(0, land.dimensions()-1, seed); };
    std::vector<position_t> positions(nb_ants);
    std::vector<ant::state> states(nb_ants, ant::unloaded);
    std::vector<std::size_t> seeds(nb_ants);

    for ( size_t i = 0; i < nb_ants; ++i ) {
        positions[i] = position_t{ gen_ant_pos(), gen_ant_pos() };
        seeds[i] = seed + i; // Graine individual por formiga
    }

    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);
    Window win("Ant Simulation", 2*land.dimensions()+10, land.dimensions()+266);
    Renderer renderer( land, phen, pos_nest, pos_food, positions );

    size_t food_quantity = 0;
    SDL_Event event;
    bool cont_loop = true;
    bool not_food_in_nest = true;
    int first_food_iteration = 0;
    std::size_t it = 0;


    std::ofstream log_file("performance_log.csv");
    // Escreve o cabeçalho das colunas
    log_file << "Iteration,Events,Ants,Evaporation,Update,Render,Blit,Total_Iter\n";
    while (cont_loop) {
        it++;
        auto t_iter_begin = hr_clock::now();

        // 1. Eventos
        auto t0 = hr_clock::now();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) cont_loop = false;
        }
        auto t1 = hr_clock::now();

        
        auto t2 = hr_clock::now();

        // D'abord il faut paralleliser ce loop, vu qu'il est le plus lourd sauf le render
        #pragma omp parallel for reduction(+:food_quantity)
        for (size_t i = 0; i < nb_ants; ++i) {
            advance_ant(land, phen, pos_nest, pos_food, positions, states, seeds, i, food_quantity);
        }
        auto t3 = hr_clock::now();

        // 3. Evaporação
        auto t4 = hr_clock::now();
        phen.do_evaporation();
        auto t5 = hr_clock::now();

        // 4. Update
        auto t6 = hr_clock::now();
        phen.update();
        auto t7 = hr_clock::now();

        // 5. Renderização
        auto t8 = hr_clock::now();
        renderer.display(win, food_quantity);
        auto t9 = hr_clock::now();

        // 6. Blit
        auto t10 = hr_clock::now();
        win.blit();
        auto t11 = hr_clock::now();

        auto t_iter_end = hr_clock::now();

        // Acúmulo de métricas [cite: 65]
        total_events += duration_ms(t1 - t0).count();
        total_ants   += duration_ms(t3 - t2).count();
        total_evap   += duration_ms(t5 - t4).count();
        total_update += duration_ms(t7 - t6).count();
        total_render += duration_ms(t9 - t8).count();
        total_blt    += duration_ms(t11 - t10).count();
        total_iter   += duration_ms(t_iter_end - t_iter_begin).count();
        nb_iters_mesurees++;

        if (not_food_in_nest && food_quantity > 0) {
            std::cout << "La première nourriture est arrivée au nid a l'iteration " << it << std::endl;
            first_food_iteration = it;
            not_food_in_nest = false;
        }

        if (it % 100 == 0) {
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "\n=== Moyennes sur " << nb_iters_mesurees << " iterations ===\n";
            std::cout << "Events      : " << total_events / nb_iters_mesurees << " ms\n";
            std::cout << "Ants        : " << total_ants   / nb_iters_mesurees << " ms\n";
            std::cout << "Evaporation : " << total_evap   / nb_iters_mesurees << " ms\n";
            std::cout << "Update      : " << total_update / nb_iters_mesurees << " ms\n";
            std::cout << "Render      : " << total_render / nb_iters_mesurees << " ms\n";
            std::cout << "Blit        : " << total_blt    / nb_iters_mesurees << " ms\n";
            std::cout << "Total iter  : " << total_iter   / nb_iters_mesurees << " ms\n";
        }
        if (it % 100 == 0) {
            // Registra as médias no arquivo de log
            log_file << it << ","
                    << total_events / nb_iters_mesurees << ","
                    << total_ants   / nb_iters_mesurees << ","
                    << total_evap   / nb_iters_mesurees << ","
                    << total_update / nb_iters_mesurees << ","
                    << total_render / nb_iters_mesurees << ","
                    << total_blt    / nb_iters_mesurees << ","
                    << total_iter   / nb_iters_mesurees << "\n";
            
            // Opcional: Garante que os dados sejam gravados no disco sem fechar o arquivo
            log_file.flush(); 
        }
    }
    log_file << "La première nourriture est arrivée au nid a l'iteration " << first_food_iteration;
    log_file.flush();
    log_file.close();
    SDL_Quit();
    return 0;
}