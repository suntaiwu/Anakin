#ifndef ANAKIN_SABER_FUNCS_IMPL_BMDNN_SCALE_H
#define ANAKIN_SABER_FUNCS_IMPL_BMDNN_SCALE_H

#include "saber/funcs/impl/impl_scale.h"

namespace anakin{

namespace saber{

template <DataType OpDtype,
    DataType inDtype,
    DataType outDtype,
    typename LayOutType_op,
    typename LayOutType_in,
    typename LayOutType_out>
class VenderScale<BM, OpDtype, inDtype, outDtype,\
    LayOutType_op, LayOutType_in, LayOutType_out> : \
    public ImplBase<
        Tensor<BM, inDtype, LayOutType_in>,
        Tensor<BM, outDtype, LayOutType_out>,
        Tensor<BM, OpDtype, LayOutType_op>,
        ScaleParam<Tensor<BM, OpDtype, LayOutType_op> > >
{
public:
    typedef Tensor<BM, inDtype, LayOutType_in> DataTensor_in;
    typedef Tensor<BM, outDtype, LayOutType_out> DataTensor_out;
    typedef Tensor<BM, OpDtype, LayOutType_op> OpTensor;
    typedef typename DataTensor_in::Dtype InDataType;
    typedef typename DataTensor_out::Dtype OutDataType;
    typedef typename OpTensor::Dtype OpDataType;

    VenderScale() {}

    ~VenderScale() {}

    virtual SaberStatus init(const std::vector<DataTensor_in *>& inputs,
                            std::vector<DataTensor_out *>& outputs,
                            ScaleParam<OpTensor>& param, Context<BM>& ctx) {

        _handle = get_bm_handle();
        return create(inputs, outputs, param, ctx);
    }

    virtual SaberStatus create(const std::vector<DataTensor_in *>& inputs,
                            std::vector<DataTensor_out *>& outputs,
                            ScaleParam<OpTensor>& param, Context<BM> &ctx) {

    }
    
    virtual SaberStatus dispatch(const std::vector<DataTensor_in*>& inputs,
                          std::vector<DataTensor_out*>& outputs,
                          ScaleParam<OpTensor>& param) {

        const InDataType in_data = *(inputs[0]->data());
        OutDataType out_data = *(outputs[0]->mutable_data());

        int input_n = inputs[0]->num();
        int input_c = inputs[0]->channel();
        int input_h = inputs[0]->height();
        int input_w = inputs[0]->width();

        int axis = (param.num_axes == 0) ? 0 : param.axis;
        int num_axes = param.num_axes >=0 ? param.num_axes : inputs[0]->shape().dims() - axis;

        int outer_dim = inputs[0]->count(0, axis);
        int inner_dim = inputs[0]->count(axis + num_axes, inputs[0]->shape().dims());
        int scale_dim = inputs[0]->count(axis, axis + num_axes);
        /* if (inputs.size() == 1) { */
        /*     CHECK_EQ(scale_dim, param.scale_w.size()) << "scale dim not valid"; */
        /* } */

        OpDataType scale_data = param.scale_w;
        bm_device_mem_t* data_extension = new bm_device_mem_t();
        int size = input_n * input_c * input_h * input_w;
        bm_malloc_device_byte(_handle, data_extension, size * sizeof(float));
        BMDNN_CHECK(bmdnn_scale_forward(_handle, in_data, scale_data,
                input_n, input_c, input_h, input_w,
                scale_dim, inner_dim, 0,
                *data_extension, out_data));
        
        if (param.bias_term) {
            OpDataType bias_data = param.scale_b;
            float* host_bias = new float[scale_dim];
            float* host_extension = new float[size];
            printf(".........\n");
//        bm_device_mem_t temp;;
//        bm_malloc_device_byte(_handle, &temp, scale_dim * sizeof(float));
//            bm_memcpy_d2s(_handle, bm_mem_from_system(host_bias), temp);
//            bm_memcpy_d2s(_handle, bm_mem_from_system(host_bias), reinterpret_cast<bm_device_mem_t>(param.scale_b));
            bm_memcpy_d2s(_handle, bm_mem_from_system(host_bias), bm_mem_from_device(&bias_data));
            int dim = inner_dim * scale_dim;
            host_bias[0] = 1;
            host_bias[1] = 2;
            for (int i = 0; i < size; ++i) {
                 int bias_dim = (i % dim) / inner_dim;
                 host_extension[i] = host_bias[bias_dim];
                 printf("%f, ", host_extension[i]);
            }
            printf("\n");
            bm_memcpy_s2d(_handle, *data_extension, bm_mem_from_system(const_cast<float *>(host_extension)));
            delete [] host_bias;
            delete [] host_extension; 
            BMDNN_CHECK(bmdnn_bias_forward(_handle, out_data, *data_extension,
                    outer_dim, scale_dim * inner_dim, out_data));
        }
        bm_free_device(_handle, *data_extension);
        return SaberSuccess;
    }
private:
    bm_handle_t _handle;
};

}
}
#endif //ANAKIN_SABER_FUNCS_IMPL_BMDNN_SCALE_H
