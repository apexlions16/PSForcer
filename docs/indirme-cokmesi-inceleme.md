# İndirme çökmesi incelemesi

PS4 üzerinde Kare düğmesine basıldığı anda oluşan CE-34878-0 hatası için iki risk giderildi:

- OpenOrbis yapısında `std::thread` yerine SDL iş parçacığı kullanıldı.
- HTTP okuma tamponu iş parçacığı yığınından heap alanına taşındı.

Ağ modülleri ve paylaşılan HTTP bağlamı iş parçacığı oluşturulmadan önce ana iş parçacığında hazırlanır. İndirilen PKG katalogdaki beklenen bayt boyutuyla doğrulanır.
