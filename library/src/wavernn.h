#ifndef WAVERNN_H
#define WAVERNN_H

#include <stdio.h>
#include <Eigen/Dense>
#include <unsupported/Eigen/CXX11/Tensor>

using namespace Eigen;

const int SPARSE_GROUP_SIZE = 8; //When pruning we use groups of 8 to reduce index
const uint8_t ROW_END_MARKER = 255;

typedef Matrix<float, Dynamic, Dynamic, RowMajor> Matrixf;
typedef Tensor<float, 3, RowMajor> Tensor3df;
typedef Tensor<float, 4, RowMajor> Tensor4df;
typedef Matrix<float, Dynamic, 1> Vectorf;
typedef Matrix<uint8_t, Dynamic, 1> Vectori8;


class CompMatrix{
    Vectorf weight;
    Vectori8 index;
    int nRows, nCols;

public:
    CompMatrix()=default;

    void read(FILE* fd, int elSize, int _nRows, int _nCols){
        int nWeights=0;
        int nIndex=0;
        nRows = _nRows;
        nCols = _nCols;

        fread(&nWeights, sizeof(int), 1, fd);

        weight.resize(nWeights);
        fread(weight.data(), elSize, nWeights, fd);

        fread(&nIndex, sizeof(int), 1, fd);
        index.resize(nIndex);
        fread(index.data(), sizeof(int8_t), nIndex, fd);
    }

    Vectorf operator*( const Vectorf& x);
};


//TODO: This should be turned into a proper class factory pattern
class TorchLayer {
    struct alignas(1) Header{
        //int size; //size of data blob, not including this header
        enum class LayerType : char { Conv1d=1, Conv2d=2, BatchNorm1d=3, Linear=4, GRU=5 } layerType;
        char name[64]; //layer name for debugging
    };

    std::vector<TorchLayer*> objects;
    void addObject( TorchLayer* o){
        objects.push_back(o);
    }

public:
    TorchLayer* loadNext( FILE* fd );

    //TODO: Turn this into variadic template function
    virtual Vectorf operator()( const Vectorf& x ){ assert(0); };
    virtual Vectorf operator()( const Vectorf& x, const Vectorf& hx ){ assert(0); };
    virtual ~TorchLayer(){
        //TorchLayer takes ownership of loaded layers. Cleanup here.
        for( auto o : objects )
            delete o;
        objects.clear();
    };
};


class Conv1dLayer : TorchLayer{
    struct alignas(1) Header{
        char elSize;  //size of each entry in bytes: 4 for float, 2 for fp16.
        bool useBias;
        int inChannels;
        int outChannels;
        int kernelSize;
    };

    Tensor3df weight;
    Vectorf bias;


    //call TorchLayer loadNext, not derived loadNext
    Conv1dLayer* loadNext( FILE* fd );

public:

    virtual Vectorf operator()( const Vectorf& x );

};

class Conv2dLayer : TorchLayer{
    struct alignas(1) Header{
        char elSize;  //size of each entry in bytes: 4 for float, 2 for fp16.
        bool useBias;
        int inChannels;
        int outChannels;
        int kernelSize1;
        int kernelSize2;
    };

    Tensor4df weight;
    Vectorf bias;


    //call TorchLayer loadNext, not derived loadNext
    Conv2dLayer* loadNext( FILE* fd );

public:

    virtual Vectorf operator()( Vectorf& x );

};

class BatchNorm1dLayer : TorchLayer{
    struct alignas(1) Header{
        char elSize;  //size of each entry in bytes: 4 for float, 2 for fp16.
        int inChannels;
    };

    Vectorf weight;
    Vectorf bias;
    Vectorf running_mean;
    Vectorf running_var;


public:
    //call TorchLayer loadNext, not derived loadNext
    BatchNorm1dLayer* loadNext( FILE* fd );

    virtual Vectorf operator()( Vectorf& x );

};


class LinearLayer : public TorchLayer{
    struct alignas(1) Header{
        char elSize;  //size of each entry in bytes: 4 for float, 2 for fp16.
        int nRows;
        int nCols;
    };

    CompMatrix mat;
    Vectorf bias;
    int nRows;
    int nCols;


public:
    LinearLayer() = default;
    //call TorchLayer loadNext, not derived loadNext
    LinearLayer* loadNext( FILE* fd );
    virtual Vectorf operator()( const Vectorf& x ) override;
};


class GRULayer : public TorchLayer{
    struct alignas(1) Header{
        char elSize;  //size of each entry in bytes: 4 for float, 2 for fp16.
        int nHidden;
        int nInput;
    };

    CompMatrix W_ir,W_iz,W_in;
    CompMatrix W_hr,W_hz,W_hn;
    Vectorf b_ir,b_iz,b_in;
    Vectorf b_hr,b_hz,b_hn;
    int nRows;
    int nCols;


public:
    GRULayer() = default;

    //call TorchLayer loadNext, not derived loadNext
    GRULayer* loadNext( FILE* fd );
    virtual Vectorf operator()( const Vectorf& x, const Vectorf& hx ) override;

};


#endif // WAVERNN_H
