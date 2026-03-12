#include <vector>
#include <iostream>
#include <random>
#include "fractal_land.hpp"
#include "ant.hpp"
#include "pheronome.hpp"
# include "renderer.hpp"
# include "window.hpp"
# include "rand_generator.hpp"

#include <chrono>
#include <iomanip>

using hr_clock = std::chrono::high_resolution_clock;
using duration_ms = std::chrono::duration<double, std::milli>;

double total_events = 0.0;
double total_ants = 0.0;
double total_evap = 0.0;
double total_update = 0.0;
double total_render = 0.0;
double total_blt = 0.0;
double total_iter = 0.0;

std::size_t nb_iters_mesurees = 0;

void advance_time( const fractal_land& land, pheronome& phen, 
                   const position_t& pos_nest, const position_t& pos_food,
                   std::vector<ant>& ants, std::size_t& cpteur )
{
    for ( size_t i = 0; i < ants.size(); ++i )
        ants[i].advance(phen, land, pos_food, pos_nest, cpteur);
    phen.do_evaporation();
    phen.update();
}

int main(int nargs, char* argv[])
{
    SDL_Init( SDL_INIT_VIDEO );
    std::size_t seed = 2026; // Graine pour la génération aléatoire ( reproductible )
    const int nb_ants = 5000; // Nombre de fourmis
    const double eps = 0.8;  // Coefficient d'exploration
    const double alpha=0.7; // Coefficient de chaos
    //const double beta=0.9999; // Coefficient d'évaporation
    const double beta=0.999; // Coefficient d'évaporation
    // Location du nid
    position_t pos_nest{256,256};
    // Location de la nourriture
    position_t pos_food{500,500};
    //const int i_food = 500, j_food = 500;    
    // Génération du territoire 512 x 512 ( 2*(2^8) par direction )
    fractal_land land(8,2,1.,1024);
    double max_val = 0.0;
    double min_val = 0.0;
    for ( fractal_land::dim_t i = 0; i < land.dimensions(); ++i )
        for ( fractal_land::dim_t j = 0; j < land.dimensions(); ++j ) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }
    double delta = max_val - min_val;
    /* On redimensionne les valeurs de fractal_land de sorte que les valeurs
    soient comprises entre zéro et un */
    for ( fractal_land::dim_t i = 0; i < land.dimensions(); ++i )
        for ( fractal_land::dim_t j = 0; j < land.dimensions(); ++j )  {
            land(i,j) = (land(i,j)-min_val)/delta;
        }
    // Définition du coefficient d'exploration de toutes les fourmis.
    ant::set_exploration_coef(eps);
    // On va créer des fourmis un peu partout sur la carte :
    std::vector<ant> ants;
    ants.reserve(nb_ants);
    auto gen_ant_pos = [&land, &seed] () { return rand_int32(0, land.dimensions()-1, seed); };
    for ( size_t i = 0; i < nb_ants; ++i )
        ants.emplace_back(position_t{gen_ant_pos(),gen_ant_pos()}, seed);
    // On crée toutes les fourmis dans la fourmilière.
    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);

    Window win("Ant Simulation", 2*land.dimensions()+10, land.dimensions()+266);
    Renderer renderer( land, phen, pos_nest, pos_food, ants );
    // Compteur de la quantité de nourriture apportée au nid par les fourmis
    size_t food_quantity = 0;
    SDL_Event event;
    bool cont_loop = true;
    bool not_food_in_nest = true;
    std::size_t it = 0;
    while (cont_loop) {
    ++it;

    auto t_iter_begin = hr_clock::now();

    auto t0 = hr_clock::now();
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            cont_loop = false;
    }
    auto t1 = hr_clock::now();

    auto t2 = hr_clock::now();
    for (size_t i = 0; i < ants.size(); ++i)
        ants[i].advance(phen, land, pos_food, pos_nest, food_quantity);
    auto t3 = hr_clock::now();

    auto t4 = hr_clock::now();
    phen.do_evaporation();
    auto t5 = hr_clock::now();

    auto t6 = hr_clock::now();
    phen.update();
    auto t7 = hr_clock::now();

    auto t8 = hr_clock::now();
    renderer.display(win, food_quantity);
    auto t9 = hr_clock::now();

    auto t10 = hr_clock::now();
    win.blit();
    auto t11 = hr_clock::now();

    auto t_iter_end = hr_clock::now();

    total_events += duration_ms(t1 - t0).count();
    total_ants   += duration_ms(t3 - t2).count();
    total_evap   += duration_ms(t5 - t4).count();
    total_update += duration_ms(t7 - t6).count();
    total_render += duration_ms(t9 - t8).count();
    total_blt    += duration_ms(t11 - t10).count();
    total_iter   += duration_ms(t_iter_end - t_iter_begin).count();

    ++nb_iters_mesurees;

    if (not_food_in_nest && food_quantity > 0) {
        std::cout << "La première nourriture est arrivée au nid a l'iteration " << it << std::endl;
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
}
    SDL_Quit();
    return 0;
}
