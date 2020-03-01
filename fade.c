#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL.h>
#include <SDL_image.h>

void Slock(SDL_Surface *screen)
{
  if ( SDL_MUSTLOCK(screen) )
    if ( SDL_LockSurface(screen) < 0 )
      return;
}

void Sulock(SDL_Surface *screen)
{
  if ( SDL_MUSTLOCK(screen) )
    SDL_UnlockSurface(screen);
}


void fade_code(SDL_Surface *im1, SDL_Surface *im2, Uint8 alpha, SDL_Surface* imOut)
{
    int pixelsCount = imOut->w * im1->h;
    int byteCount = imOut->format->BytesPerPixel * pixelsCount;
    
    Uint8 *A = (Uint8*) im1->pixels;
    Uint8 *B = (Uint8*) im2->pixels;
    Uint8 *out = (Uint8*) imOut->pixels;
    Uint8 *end = out + byteCount;
    
    for(; out != end; out++,A++,B++)
        *out = (Uint8)(( (*A) * alpha + (*B) * (128 - alpha))/128) ;
}


void fade_mmx(SDL_Surface* im1,SDL_Surface* im2,Uint8 alpha, SDL_Surface* imOut)
{
    int pixelsCount = imOut->w * im1->h;
    Uint32 *A = (Uint32*) im1->pixels; //jeden pixel 32 bity
    Uint32 *B = (Uint32*) im2->pixels; //(8 bit alpha, 8 bit r, 8 bit g, 8 bit b)
    Uint32 *out = (Uint32*) imOut->pixels;
    Uint32 *end = out + pixelsCount;
    Uint32 alphaE = (((Uint32)alpha << 24) | ((Uint32)alpha << 16) | ((Uint32)alpha << 8) | (alpha));
    Uint32 invAlphaE = ~alphaE;

    asm volatile(
        "\n\t movd %0,%%mm3" 
        "\n\t movd %1,%%mm4"

        "\n\t pxor %%mm0, %%mm0"

        "\n\t punpcklbw %%mm0, %%mm3"
        "\n\t punpcklbw %%mm0, %%mm4"
        : :"m" (alphaE), "m" (invAlphaE)
        );


    for(; out != end; out++,A++,B++){
    asm volatile(
        "\n\t movd %1,%%mm1"
        "\n\t movd %2,%%mm2"

        "\n\t punpcklbw %%mm0, %%mm1"
        "\n\t punpcklbw %%mm0, %%mm2"

        "\n\t pmullw %%mm3, %%mm1"
        "\n\t pmullw %%mm4, %%mm2"

        "\n\t psrlw $8, %%mm1"
        "\n\t psrlw $8, %%mm2"
        
        "\n\t paddb %%mm2, %%mm1"
        
        "\n\t packuswb %%mm1, %%mm1"
        "\n\t movd %%mm1,%0"
        :"=r"(*out)
        :"r" (*A), "r" (*B) 
        );
    }
    asm volatile(
        "\n\t emms" : :
        );
}


int main(int argc, char *argv[])
{
    char *file1 = "image1.jpg",*file2 = "image2.jpg";
    
    if(argc == 3){
        file1 = argv[1];
        file2 = argv[2];
    }
    
    if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0 ){
        printf("Unable to init SDL: %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
    
    SDL_Surface *screen;
    screen = SDL_SetVideoMode( 800, 600,32, SDL_HWSURFACE | SDL_DOUBLEBUF);
    
    if ( screen == NULL ){
        printf("Unable to set %dx%d video: %s\n",800, 600, SDL_GetError());
        exit(1);
    }
  
    SDL_Surface *image1,*image2,*output;
    SDL_Surface *temp;
     
    temp = IMG_Load(file1);
    if (temp == NULL) 
    {
        printf("Unable to load bitmap: %s\n", SDL_GetError());
        return 1;
    }
    image1 = SDL_DisplayFormat(temp);
    SDL_FreeSurface(temp);

    temp = IMG_Load(file2);
    if (temp == NULL) 
    {
        printf("Unable to load bitmap: %s\n", SDL_GetError());
        return 1;
    }
    image2 = SDL_DisplayFormat(temp);
    SDL_FreeSurface(temp);
    
    if( image1->w != image2->w || image1->h != image2->h )
    {
        printf("Bad bitmaps sizes\n");
        return 1;
    }
    
    temp = SDL_CreateRGBSurface(
        SDL_SWSURFACE, image1->w, image1->h,
        image1->format->BitsPerPixel,
        image1->format->Rmask, image1->format->Gmask, 
        image1->format->Bmask, image1->format->Amask 
    );
    
    output = SDL_DisplayFormat(temp);
    SDL_FreeSurface(temp);
    
    SDL_Rect src, dest;
    
    src.x = 0;
    src.y = 0;
    src.w = output->w;
    src.h = output->h;
     
    dest.x = 0;
    dest.y = 0;
    dest.w = output->w;
    dest.h = output->h;
    
    int maxFPS = 0;
    int FPS = 0;
    int lastTimeFPS = 0;
    
    int done = false;
    int mode = 2, lastMode = 0;
    int alpha;
    Uint8 f = 0;
    char buffer[32];
    
    while(!done)
    {
        SDL_Event event;
        
        while ( SDL_PollEvent(&event) )
        {
            if ( event.type == SDL_QUIT )
                done = true;
        
            if ( event.type == SDL_KEYDOWN )
            {
                if ( event.key.keysym.sym == SDLK_ESCAPE )
                    done = true;
                if ( event.key.keysym.sym == SDLK_1 )
                    mode = 1;
                if ( event.key.keysym.sym == SDLK_2 )
                    mode = 2;
            }
        }
     
        alpha = f<0x80 ? (f&0x7f) : 0x80-(f&0x7f);
        f++;

        switch(mode)
        {
            case 1:
                fade_code(image1, image2, alpha, output);
                break;
            default:
                fade_mmx(image1, image2, alpha, output);
                break;
        }
        
        int currentTime = SDL_GetTicks();
        
        Slock(screen);
        SDL_BlitSurface(output, &src, screen, &dest);
        Sulock(screen);
        
        SDL_Flip(screen);
        FPS++;
        
        if ( currentTime - lastTimeFPS >= 1000 )
        {
            if (FPS > maxFPS)
                maxFPS = FPS;
            
            if (lastMode != mode)
                maxFPS = 0;
            
            snprintf( buffer, sizeof(buffer), "%d FPS (max:%d) [%d]", FPS, maxFPS, mode );
            SDL_WM_SetCaption( buffer,0 );
            FPS = 0;
            lastTimeFPS = currentTime;
            lastMode = mode;
        } 
    }
    
    SDL_Quit();
    
    return 0;
}

