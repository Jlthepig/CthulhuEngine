#pragma  once



namespace Cthulhu::Core
{
    class Audio
    {
        public:
        static void init();
        static void shutdown();

        static void playSound(const char* filePath);
        private:
        
    };
}