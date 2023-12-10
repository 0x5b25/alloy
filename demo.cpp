#include <veldrid/backend/Backends.hpp>


int main(void){
    //return 0;
    {
        Veldrid::GraphicsDevice::Options opt{};

        auto dev = Veldrid::CreateDX12GraphicsDevice(opt, nullptr);
    }

    return 0;

}
