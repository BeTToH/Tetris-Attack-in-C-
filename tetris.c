#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <windows.h>
#include <gl/gl.h>
#include "SOIL.h"
#include "pacman.h"
#include <time.h>

//=========================================================
// Tamanho de cada bloco da matriz do jogo
#define bloco 70
// Tamanho da matriz do jogo
#define N 20
#define L 14
#define C 6


// Tamanho de cada bloco da matriz do jogo na tela
#define TAM 0.1f
//Funções que convertem a linha e coluna da matriz em uma coordenada de [-1,1]
#define MAT2X(j) ((j)*0.1f-0.4)
#define MAT2Y(i) (0.4-(i)*0.1f)

//=========================================================
// Estruturas usadas para controlar o jogo
struct TPoint{
    int x,y;
};

const struct TPoint direcoes[4] = {{1,0},{0,1},{-1,0},{0,-1}};

struct TPacman{
    int status;
    int xi,yi,x,y;
    int direcao,passo,parcial;
    int pontos;
    int vivo;
};

struct TPhantom{
    int xi,yi,x,y;
    int direcao,passo,parcial;
	int indice_atual;
};

struct TVertice{
    int x,y;
    int vizinhos[4];
};

struct TCenario{
    int mapa[L][C];
    int nro_pastilhas;
    int NV;
    struct TVertice *grafo;
};

//==============================================================
// Texturas
//==============================================================

GLuint pacmanTex2d[1];
GLuint pontosTex2d[10];
GLuint mapaTex2d[5];

GLuint telaStart, telaGameOver;

static void desenhaSprite(float coluna,float linha, GLuint tex);
static GLuint carregaArqTextura(char *str);
static void desenhaTipoTela(float x, float y, float tamanho, GLuint tex);

// Função que carrega todas as texturas do jogo
void carregaTexturas(){
    int i;
    char str[50];
    for(i=0; i<10; i++){
        sprintf(str,".//Sprites//sprites%d.png",i);
        pontosTex2d[i] = carregaArqTextura(str);
    }

    for(i=0; i<1; i++){
        sprintf(str,".//Sprites//grade%d.png",i);
        pacmanTex2d[i] = carregaArqTextura(str);
    }

    for(i=0; i<6; i++){
        sprintf(str,".//Sprites//bloco%d.png",i);
        mapaTex2d[i] = carregaArqTextura(str);
    }

    telaStart = carregaArqTextura(".//Sprites//start.png");
    telaGameOver = carregaArqTextura(".//Sprites//gameover.png");;

}

// Função que carrega um arquivo de textura do jogo
static GLuint carregaArqTextura(char *str){
    // http://www.lonesock.net/soil.html
    GLuint tex = SOIL_load_OGL_texture
        (
            str,
            SOIL_LOAD_AUTO,
            SOIL_CREATE_NEW_ID,
            SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y |
            SOIL_FLAG_NTSC_SAFE_RGB | SOIL_FLAG_COMPRESS_TO_DXT
        );

    /* check for an error during the load process */
    if(0 == tex){
        printf( "SOIL loading error: '%s'\n", SOIL_last_result() );
    }

    return tex;
}

// Função que recebe uma linha e coluna da matriz e um código
// de textura e desenha um quadrado na tela com essa textura
void desenhaSprite(float coluna,float linha, GLuint tex){
    glColor3f(1.0, 1.0, 1.0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f,0.0f); glVertex2f(coluna, linha);
        glTexCoord2f(1.0f,0.0f); glVertex2f(coluna+TAM, linha);
        glTexCoord2f(1.0f,1.0f); glVertex2f(coluna+TAM, linha+TAM);
        glTexCoord2f(0.0f,1.0f); glVertex2f(coluna, linha+TAM);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

}

void desenhaTipoTela(float x, float y, float tamanho, GLuint tex){

    glPushMatrix();

    glColor3f(1.0, 1.0, 1.0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);

    glEnable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f,1.0f); glVertex2f(x - tamanho, y + tamanho);
        glTexCoord2f(1.0f,1.0f); glVertex2f(x + tamanho, y + tamanho);
        glTexCoord2f(1.0f,0.0f); glVertex2f(x + tamanho, y - tamanho);
        glTexCoord2f(0.0f,0.0f); glVertex2f(x - tamanho, y - tamanho);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
}

void desenhaTela(int tipo){
    if(tipo == 0)
        desenhaTipoTela(0, 0, 1.0, telaStart);
    else
        desenhaTipoTela(0, 0, 1.0, telaGameOver);
}

//==============================================================
// Cenario
//==============================================================

static int cenario_EhCruzamento(int x, int y, Cenario* cen);
static int cenario_VerificaDirecao(int mat[L][C], int y, int x, int direcao);
static void cenario_constroiGrafo(Cenario* cen);

// Função que carrega o cenário
Cenario* cenario_carrega(){
    int i,j;

    Cenario* cen = malloc(sizeof(Cenario));
    cen->nro_pastilhas = 0;

    for(i=0; i<13; i++){
        for(j=0; j<C; j++)
            cen->mapa[i][j] = 5;
    }
    for(i=6; i<13; i++){
        for(j=0; j<C; j++){
            cen->mapa[i][j] = rand() %4;

        }}

    return cen;
}

// Libera os dados associados ao cenário
void cenario_destroy(Cenario* cen){
    free(cen->grafo);
    free(cen);
}

// Percorre a matriz do jogo desenhando os sprites
void cenario_desenha(Cenario* cen){
    int i,j;
    for(i=1; i<13; i++){
        for(j=0; j<C; j++){
            if(cen->mapa[i][j]!=5)
            desenhaSprite(MAT2X(j),MAT2Y(i),mapaTex2d[cen->mapa[i][j]]);

     }}
}


void quebra_bloco(Cenario *cen, Pacman *pac){
    int i, j, cont, cont_c, k;
    for(i=0; i<11; i++){
        for(j=0; j<C; j++){
            cont = 1;
            while(cen->mapa[i+cont][j] == cen->mapa[i][j]){
                cont++;
            }if(cont >= 3){
            for(k = i; k < i+cont; k++){
                cen->mapa[k][j] = 5;
                pac->pontos += (cont-2)*50;
            }}
        }}

            for(i=0; i<13; i++){
                for(j=0; j<5; j++){
                    cont_c = 1;
                    while(cen->mapa[i][j+cont_c] == cen->mapa[i][j]){
                        cont_c++;
                        }if(cont_c >= 3){
                        for(k = j; k < j+cont_c; k++){
                            cen->mapa[i][k] = 5;
                            pac->pontos += (cont_c - 2)*50;
            }
        }}
            }

            }

void desce_bloco(Cenario *cen){
      int i, j;
      for(i=2; i<13; i++){
        for(j=0; j<7; j++){
            if(cen->mapa[i][j] == 5){
                cen->mapa[i][j] = cen->mapa[i-1][j];
                cen->mapa[i-1][j] = 5;
            }
            }
        }
}

// Função que verifica se é possivel andar em uma determinada direção
static int cenario_VerificaDirecao(int mat[L][C], int y, int x, int direcao){
    int xt = x;
    int yt = y;
    while(mat[yt + direcoes[direcao].y][xt + direcoes[direcao].x] == 0){ //não é parede...
        yt = yt + direcoes[direcao].y;
        xt = xt + direcoes[direcao].x;
    }

    if(mat[yt + direcoes[direcao].y][xt + direcoes[direcao].x] < 0)
        return -1;
    else
        return mat[yt + direcoes[direcao].y][xt + direcoes[direcao].x] - 1;
}

//==============================================================
// Pacman
//==============================================================

static int pacman_eh_invencivel(Pacman *pac);
static void pacman_morre(Pacman *pac);
static void pacman_pontos_fantasma(Pacman *pac);
static void pacman_AnimacaoMorte(float coluna,float linha,Pacman* pac);

// Função que inicializa os dados associados ao pacman
Pacman* pacman_create(int x, int y){
    Pacman* pac = malloc(sizeof(Pacman));
    if(pac != NULL){
        pac->pontos = 0;
        pac->passo = 4;
        pac->vivo = 1;
        pac->status = 0;
        pac->direcao = 0;
        pac->parcial = 0;
        pac->xi = x;
        pac->yi = y;
        pac->x = x;
        pac->y = y;
    }
    return pac;
}

// Função que libera os dados associados ao pacman
void pacman_destroy(Pacman *pac){
    free(pac);
}

// Função que verifica se o pacman está vivo ou não
int pacman_vivo(Pacman *pac){
    if(pac->vivo)
        return 1;
}

// Função que verifica se o pacman pode ir para uma nova direção escolhida
void pacman_AlteraDirecao(Pacman *pac, int direcao, Cenario *cen){
    if(direcao == 1 && pac->y < 12){
        pac->y++;
    }
     if(direcao == 3 && pac->y > 0){
        pac->y--;
    }
     if(direcao == 0 && pac->x  < 4){
        pac->x++;
    }
     if(direcao == 2 && pac->x > 0){
        pac->x--;
    }
    int recebe;
       if(direcao == 4){
        recebe = cen->mapa[pac->y][pac->x];
        cen->mapa[pac->y][pac->x] = cen->mapa[pac->y][pac->x+1];
        cen->mapa[pac->y][pac->x+1] = recebe;
    }
    }


// Atualiza a posição do pacman
void pacman_movimenta(Pacman *pac, Cenario *cen){
    if(pac->vivo == 0)
        return;

    // Incrementa a sua posição dentro de uma célula da matriz ou muda de célula
    if(cen->mapa[pac->y + direcoes[pac->direcao].y][pac->x + direcoes[pac->direcao].x] <= 4){ //&& cen->mapa[pac->y + direcoes[pac->direcao].y][pac->x + direcoes[pac->direcao].x] >=0){//não é parede...
        if(pac->direcao < 2){
            if(pac->parcial >= bloco){
                pac->x += direcoes[pac->direcao].x;
                pac->y += direcoes[pac->direcao].y;
                pac->parcial = 0;
           }
        }else{
            if(pac->parcial <= -bloco){
                pac->x += direcoes[pac->direcao].x;
                pac->y += direcoes[pac->direcao].y;
                pac->parcial = 0;
           }
        }
    }
    }

void grade_work(Pacman *pac, int direcao, Cenario *cen){
    int recebe;
    if(direcao == 4){
        recebe = cen->mapa[pac->y][pac->x];
        cen->mapa[pac->y][pac->x] = cen->mapa[pac->y][pac->x+1];
        cen->mapa[pac->y][pac->x+1] = recebe;
    }
}

// Função que desenha o pacman
void pacman_desenha(Pacman *pac){
    float linha, coluna;
    float passo = (pac->parcial/(float)bloco);
    //Verifica a posição
    if(pac->direcao == 0 || pac->direcao == 2){
        linha = pac->y;
        coluna = pac->x + passo;
    }else{
        linha = pac->y + passo;
        coluna = pac->x;
    }

    if(pac->vivo){
        desenhaSprite(MAT2X(coluna),MAT2Y(linha), pacmanTex2d[0]);
        desenhaSprite(MAT2X(coluna+1),MAT2Y(linha), pacmanTex2d[0]);
}
}

static void pacman_morre(Pacman *pac){
    if(pac->vivo){
        pac->vivo = 0;
        }
}


void gera_linha(Cenario *cen, Pacman *pac){
    if(pac->vivo==1){
    for(int i = 0; i < L; i++){
        for(int j = 0; j < C; j++){
            if(cen->mapa[0][j] != 5){
                pacman_morre(pac);
            }
            cen->mapa[i-1][j] = cen->mapa[i][j];
        }
        }
    for(int k=0; k<C; k++){
        cen->mapa[12][k] = rand()%4;
        if(k<4){
        if(cen->mapa[12][k] == cen->mapa[12][k+1] && cen->mapa[12][k] == cen->mapa[12][k+2])
            cen->mapa[12][k+1] = rand()%4;
    }
        if(cen->mapa[11][k] == cen->mapa[10][k]){
            int n = rand()%4;
            while(n == cen->mapa[11][k]){
                n = rand()%4;}
            cen->mapa[12][k] = n;
        }
     }
}
}
