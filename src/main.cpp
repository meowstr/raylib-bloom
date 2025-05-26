#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdio.h>

template < typename T > void copy_index( T * arr, int from, int to )
{
    memcpy( arr + to, arr + from, sizeof( T ) );
}

static Rectangle MarginRec( Rectangle r, float v )
{
    return Rectangle{ r.x + v, r.y + v, r.width - v * 2, r.height - v * 2 };
}

static void
DrawTextCentered( const char * text, int x, int y, int size, Color color )
{
    int width = MeasureText( text, size );
    DrawText( text, x - width / 2, y, size, color );
}

static void
DrawTextRight( const char * text, int x, int y, int size, Color color )
{
    int width = MeasureText( text, size );
    DrawText( text, x - width, y, size, color );
}

#define BLOOM_MIPS 8

static struct {
    struct {
        Shader s;
        float exposure;

        struct {
            int exposure;
        } locs;

    } mix_shader;

    struct {
        Shader s;
        Vector2 srcResolution;

        struct {
            int srcResolution;
        } locs;

    } downsample_shader;

    struct {
        Shader s;
        float filterRadius;

        struct {
            int filterRadius;
        } locs;
    } upsample_shader;

    RenderTexture2D fb[ BLOOM_MIPS ];
} intern;

// NOTE: copied from LoadRenderTexture, mostly
RenderTexture2D LoadHdrRenderTexture( int width, int height )
{
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer

    if ( target.id > 0 ) {
        rlEnableFramebuffer( target.id );

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(
            NULL,
            width,
            height,
            PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
            1
        );
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
        target.texture.mipmaps = 1;

        // Create depth renderbuffer/texture
        target.depth.id = rlLoadTextureDepth( width, height, true );
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19; // DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach color texture and depth renderbuffer/texture to FBO
        rlFramebufferAttach(
            target.id,
            target.texture.id,
            RL_ATTACHMENT_COLOR_CHANNEL0,
            RL_ATTACHMENT_TEXTURE2D,
            0
        );
        rlFramebufferAttach(
            target.id,
            target.depth.id,
            RL_ATTACHMENT_DEPTH,
            RL_ATTACHMENT_RENDERBUFFER,
            0
        );

        // Check if fbo is complete with attachments (valid)
        if ( rlFramebufferComplete( target.id ) )
            TRACELOG(
                LOG_INFO,
                "FBO: [ID %i] Framebuffer object created successfully",
                target.id
            );

        rlDisableFramebuffer();
    } else
        TRACELOG( LOG_WARNING, "FBO: Framebuffer object can not be created" );

    // AHHHHHHHHHHHHHHHHHHHHHHHH
    SetTextureFilter( target.texture, TEXTURE_FILTER_BILINEAR );
    SetTextureWrap( target.texture, TEXTURE_WRAP_CLAMP );

    return target;
}

static void update_downsample_shader()
{
    SetShaderValue(
        intern.downsample_shader.s,
        intern.downsample_shader.locs.srcResolution,
        &intern.downsample_shader.srcResolution,
        SHADER_UNIFORM_VEC2
    );
}

static void update_upsample_shader()
{
    SetShaderValue(
        intern.upsample_shader.s,
        intern.upsample_shader.locs.filterRadius,
        &intern.upsample_shader.filterRadius,
        SHADER_UNIFORM_FLOAT
    );
}

static void update_mix_shader()
{
    SetShaderValue(
        intern.mix_shader.s,
        intern.mix_shader.locs.exposure,
        &intern.mix_shader.exposure,
        SHADER_UNIFORM_FLOAT
    );
}

static void init_bloom( int width, int height )
{
    intern.downsample_shader.s = LoadShader( 0, "../../res/downsample.fs" );
    intern.upsample_shader.s = LoadShader( 0, "../../res/upsample.fs" );
    intern.mix_shader.s = LoadShader( 0, "../../res/mix.fs" );

    intern.downsample_shader.locs.srcResolution =
        GetShaderLocation( intern.downsample_shader.s, "srcResolution" );
    intern.upsample_shader.locs.filterRadius =
        GetShaderLocation( intern.upsample_shader.s, "filterRadius" );
    intern.mix_shader.locs.exposure =
        GetShaderLocation( intern.mix_shader.s, "exposure" );

    // generate all the mips
    for ( int i = 0; i < BLOOM_MIPS; i++ ) {
        intern.fb[ i ] = LoadHdrRenderTexture( width, height );
        width /= 2;
        height /= 2;
    }
}

static void downsample( int i )
{
    BeginTextureMode( intern.fb[ i + 1 ] );
    ClearBackground( BLACK );

    BeginShaderMode( intern.downsample_shader.s );
    intern.downsample_shader.srcResolution = Vector2{
        (float) intern.fb[ i ].texture.width,
        (float) intern.fb[ i ].texture.height
    };
    update_downsample_shader();

    DrawTexturePro(
        intern.fb[ i ].texture,
        Rectangle{
            0,
            0,
            (float) intern.fb[ i ].texture.width,
            (float) -intern.fb[ i ].texture.height
        },
        Rectangle{
            0,
            0,
            (float) intern.fb[ i + 1 ].texture.width,
            (float) intern.fb[ i + 1 ].texture.height

        },
        Vector2{ 0, 0 },
        0,
        WHITE
    );
    EndShaderMode();

    EndTextureMode();
}

static void upsample( int i )
{
    BeginTextureMode( intern.fb[ BLOOM_MIPS - 2 - i ] );
    BeginShaderMode( intern.upsample_shader.s );
    intern.upsample_shader.filterRadius = 0.003f;
    update_upsample_shader();

    DrawTexturePro(
        intern.fb[ BLOOM_MIPS - 1 - i ].texture,
        Rectangle{
            0,
            0,
            (float) intern.fb[ BLOOM_MIPS - 1 - i ].texture.width,
            (float) -intern.fb[ BLOOM_MIPS - 1 - i ].texture.height
        },
        Rectangle{
            0,
            0,
            (float) intern.fb[ BLOOM_MIPS - 2 - i ].texture.width,
            (float) intern.fb[ BLOOM_MIPS - 2 - i ].texture.height

        },
        Vector2{ 0, 0 },
        0,
        WHITE
    );
    EndShaderMode();
    EndTextureMode();
}

static void do_bloom(
    RenderTexture2D output,
    RenderTexture2D input_base,
    RenderTexture2D input_emission,
    float bloom_amount,
    float exposure_amount
)
{
    // render input emission first mip
    BeginTextureMode( intern.fb[ 0 ] );
    DrawTexturePro(
        input_emission.texture,
        Rectangle{
            0,
            0,
            (float) input_emission.texture.width,
            (float) -input_emission.texture.height
        },
        Rectangle{
            0,
            0,
            (float) intern.fb[ 0 ].texture.width,
            (float) -intern.fb[ 0 ].texture.height
        },
        Vector2{ 0, 0 },
        0,
        ColorBrightness( WHITE, bloom_amount ) // TODO: properly multiply color to exceed WHITE with a shader
    );
    EndTextureMode();

    // downsample to each mip
    for ( int i = 0; i < BLOOM_MIPS - 1; i++ ) {
        downsample( i );
    }

    // upsample to each mip
    BeginBlendMode( BLEND_ADDITIVE );
    for ( int i = 0; i < BLOOM_MIPS - 1; i++ ) {
        upsample( i );
    }
    EndBlendMode();

    // combine first mip with base input
    BeginBlendMode( BLEND_ADDITIVE );
    BeginTextureMode( intern.fb[ 0 ] );
    DrawTextureRec(
        input_base.texture,
        Rectangle{
            0,
            0,
            (float) intern.fb[ 0 ].texture.width,
            (float) -intern.fb[ 0 ].texture.height
        },
        Vector2{ 0, 0 },
        WHITE
    );
    EndTextureMode();
    EndBlendMode();

    // render to output with exposure
    BeginTextureMode( output );
    ClearBackground( BLACK );
    BeginShaderMode( intern.mix_shader.s );
    intern.mix_shader.exposure = exposure_amount;
    update_mix_shader();
    DrawTextureRec(
        intern.fb[ 0 ].texture,
        Rectangle{
            0,
            0,
            (float) intern.fb[ 0 ].texture.width,
            (float) -intern.fb[ 0 ].texture.height
        },
        Vector2{ 0, 0 },
        WHITE
    );
    EndShaderMode();
    EndTextureMode();
}

static struct {
    RenderTexture2D base_fb;
    RenderTexture2D emission_fb;
    RenderTexture2D final_fb;
} state;

static void draw_frame_emissions()
{
    BeginTextureMode( state.emission_fb );
    ClearBackground( BLACK );

    DrawTextCentered( "MEOWW", 400, 400 - 20, 40, WHITE );

    // DrawRectangle( 100, 100, 100, 100, GREEN );

    DrawCircle( 600, 600, 100, RED );

    EndTextureMode();
}

static void draw_frame()
{
    BeginTextureMode( state.base_fb );
    ClearBackground( BLACK );

    static char buffer[ 1024 ];

    snprintf( buffer, 1024, "%d FPS", GetFPS() );
    DrawTextRight( buffer, 800, 0, 20, RED );

    DrawTextCentered( "MEOWW", 400, 400 - 20, 40, WHITE );

    DrawRectangle( 100, 100, 100, 100, GREEN );

    DrawCircle( 600, 600, 100, RED );

    EndTextureMode();
}

static void init()
{
    state.base_fb = LoadRenderTexture( 800, 800 );
    state.emission_fb = LoadRenderTexture( 800, 800 );
    state.final_fb = LoadRenderTexture( 800, 800 );

    init_bloom( 800, 800 );
}

static void loop()
{
    draw_frame();
    draw_frame_emissions();

    do_bloom( state.final_fb, state.base_fb, state.base_fb, 0, 1 );
    //do_bloom( state.final_fb, state.base_fb, state.emission_fb, 0, 1 );

    BeginDrawing();
    ClearBackground( BLACK );
    DrawTextureRec(
        state.final_fb.texture,
        Rectangle{ 0, 0, 800, -800 },
        Vector2{ 0, 0 },
        WHITE
    );
    EndDrawing();
}

#ifdef EMSCRIPTEN
extern "C" {
typedef void ( *em_callback_func )( void );
void emscripten_set_main_loop(
    em_callback_func func,
    int fps,
    bool simulate_infinite_loop
);
}
#endif

int main()
{
    InitWindow( 800, 800, "sandbox" );

    init();

#ifdef EMSCRIPTEN
    emscripten_set_main_loop( loop, 0, 1 );
#else
    SetTargetFPS( 60 );
    while ( !WindowShouldClose() ) {
        loop();
    }
#endif

    CloseWindow();

    return 0;
}