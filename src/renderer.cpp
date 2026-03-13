#include <limits>
#include <algorithm>
#include <vector>
#include <omp.h>
#include "renderer.hpp"

Renderer::Renderer( const fractal_land& land, const pheronome& phen, 
                    const position_t& pos_nest, const position_t& pos_food,
                    const std::vector<position_t>& ant_positions )
    :   m_ref_land( land ),
        m_land( nullptr ),
        m_phen_texture( nullptr ),
        m_ref_phen( phen ),
        m_pos_nest( pos_nest ),
        m_pos_food( pos_food ),
        m_ref_ant_positions( ant_positions )
{}

Renderer::~Renderer() {
    if ( m_land != nullptr ) SDL_DestroyTexture( m_land );
    if ( m_phen_texture != nullptr ) SDL_DestroyTexture( m_phen_texture );
}

void Renderer::display( Window& win, std::size_t const& contador )
{
    SDL_Renderer* renderer = SDL_GetRenderer( win.get() );
    int dim = m_ref_land.dimensions();

    // 1. Inicialização de Texturas
    if ( m_land == nullptr ) {
        // (Mantendo sua lógica original de criação do terreno apenas uma vez)
        SDL_Surface* temp_surface = SDL_CreateRGBSurface(0, dim, dim, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        double min_h = 1.0, max_h = 0.0; // Simplificado considerando normalização [cite: 62]
        
        for ( int i = 0; i < dim; ++i )
            for ( int j = 0; j < dim; ++j ) {
                double c = 255. * m_ref_land(i, j);
                Uint32* pixel = (Uint32*) ((Uint8*)temp_surface->pixels + j * temp_surface->pitch + i * sizeof(Uint32));
                *pixel = SDL_MapRGBA( temp_surface->format, static_cast<Uint8>(c), static_cast<Uint8>(c), static_cast<Uint8>(c), 255 );
            }
        m_land = SDL_CreateTextureFromSurface( renderer, temp_surface );
        SDL_FreeSurface( temp_surface );

        // Criar textura dinâmica para os feromônios
        m_phen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, dim, dim);
        SDL_SetTextureBlendMode(m_phen_texture, SDL_BLENDMODE_BLEND);
    }

    // 2. Limpar a tela
    SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
    SDL_RenderClear( renderer );

    // 3. Desenhar Terrenos (Fundo)
    SDL_Rect r_left{0, 0, dim, dim};
    SDL_Rect r_right{dim + 10, 0, dim, dim};
    SDL_RenderCopy( renderer, m_land, nullptr, &r_left );
    SDL_RenderCopy( renderer, m_land, nullptr, &r_right );

    // 4. PARALELIZAÇÃO: Processamento de Feromônios (Lado Direito)
    std::vector<Uint32> pixels(dim * dim, 0);
    
    #pragma omp parallel for collapse(2)
    for ( int i = 0; i < dim; ++i ) {
        for ( int j = 0; j < dim; ++j ) {
            double r_val = std::min( 1., (double)m_ref_phen( i, j )[0] );
            double g_val = std::min( 1., (double)m_ref_phen( i, j )[1] );
            
            if ( r_val > 0.01 || g_val > 0.01 ) {
                Uint8 r = static_cast<Uint8>( r_val * 255 );
                Uint8 g = static_cast<Uint8>( g_val * 255 );
                // Formato ARGB8888: Alpha sempre 255 para visibilidade
                pixels[j * dim + i] = (255 << 24) | (r << 16) | (g << 8);
            }
        }
    }
    SDL_UpdateTexture(m_phen_texture, nullptr, pixels.data(), dim * sizeof(Uint32));
    SDL_RenderCopy(renderer, m_phen_texture, nullptr, &r_right);

    // 5. Desenhar Formigas (Lado Esquerdo)
    // Usamos SDL_RenderDrawPoints para evitar milhares de chamadas individuais
    std::vector<SDL_Point> points(m_ref_ant_positions.size());
    for(size_t i = 0; i < m_ref_ant_positions.size(); ++i) {
        points[i] = { static_cast<int>(m_ref_ant_positions[i].x), 
                      static_cast<int>(m_ref_ant_positions[i].y) };
    }
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderDrawPoints(renderer, points.data(), points.size());

    // 6. Curva de Enfouragement (Mantida sequencial por ser pequena)
    m_curve.push_back(contador);
    if ( m_curve.size() > 1 ) {
        SDL_SetRenderDrawColor( renderer, 255, 255, 127, 255 );
        double max_val = *std::max_element(m_curve.begin(), m_curve.end());
        double h_scale = 256.0 / std::max(max_val, 1.0);
        double step = (double)win.size().first / m_curve.size();
        int y_base = win.size().second - 1;

        for ( size_t i = 0; i < m_curve.size() - 1; ++i ) {
            SDL_RenderDrawLine(renderer, i*step, y_base - m_curve[i]*h_scale, (i+1)*step, y_base - m_curve[i+1]*h_scale);
        }
    }

    SDL_RenderPresent( renderer );
}