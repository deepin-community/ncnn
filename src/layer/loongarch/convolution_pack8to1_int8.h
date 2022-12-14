// yala is pleased to support the open source community by making ncnn available.
//
//
// Copyright (C) 2022 yala <zhaojunchao@loongson.cn>;<junchao82@qq.com>. All rights reserved.
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

static void convolution_pack8to1_int8_lsx(const Mat& bottom_blob, Mat& top_blob, const Mat& weight_data_int8, int kernel_w, int kernel_h, int dilation_w, int dilation_h, int stride_w, int stride_h, const Option& opt)
{
    int w = bottom_blob.w;
    int channels = bottom_blob.c;

    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    const int maxk = kernel_w * kernel_h;

    // kernel offsets
    std::vector<int> _space_ofs(maxk);
    int* space_ofs = &_space_ofs[0];
    {
        int p1 = 0;
        int p2 = 0;
        int gap = w * dilation_h - kernel_w * dilation_w;
        for (int i = 0; i < kernel_h; i++)
        {
            for (int j = 0; j < kernel_w; j++)
            {
                space_ofs[p1] = p2;
                p1++;
                p2 += dilation_w;
            }
            p2 += gap;
        }
    }

    // num_output
    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        int* outptr = top_blob.channel(p);

        for (int i = 0; i < outh; i++)
        {
            for (int j = 0; j < outw; j++)
            {
                __m128i _sum = __lsx_vreplgr2vr_w(0);

                const signed char* kptr = weight_data_int8.channel(p);

                // channels
                for (int q = 0; q < channels; q++)
                {
                    const Mat m = bottom_blob.channel(q);
                    const signed char* sptr = m.row<const signed char>(i * stride_h) + j * stride_w * 8;

                    for (int k = 0; k < maxk; k++)
                    {
                        __m128i _val = __lsx_vld(sptr + space_ofs[k] * 8, 0);
                        __m128i _val16 = __lsx_vilvl_b(__lsx_vslti_b(_val, 0), _val);

                        __m128i _w = __lsx_vld(kptr, 0);
                        __m128i _w16 = __lsx_vilvl_b(__lsx_vslti_b(_w, 0), _w);

                        __m128i _s0 = __lsx_vmul_h(_val16, _w16);

                        _sum = __lsx_vadd_w(_sum, __lsx_vhaddw_w_h(_s0, _s0));

                        kptr += 8;
                    }
                }

                outptr[j] = __lsx_reduce_add_w(_sum);
            }

            outptr += outw;
        }
    }
}
