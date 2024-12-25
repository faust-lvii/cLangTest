const ghost = document.getElementById('ghost');
const ghosts = [];
const maxGhosts = 5;

// Ana hayaleti fare imlecini takip etmesi için ayarlıyoruz
document.addEventListener('mousemove', (e) => {
    ghost.style.left = e.clientX + 'px';
    ghost.style.top = e.clientY + 'px';
});

// Her 2 saniyede bir yeni hayalet oluştur
setInterval(() => {
    if (ghosts.length < maxGhosts) {
        createGhost();
    }
}, 2000);

function createGhost() {
    const newGhost = document.createElement('div');
    newGhost.className = 'ghost';
    newGhost.innerHTML = '👻';
    document.body.appendChild(newGhost);
    
    // Rastgele başlangıç pozisyonu
    const startX = Math.random() * window.innerWidth;
    const startY = Math.random() * window.innerHeight;
    
    newGhost.style.left = startX + 'px';
    newGhost.style.top = startY + 'px';
    
    // Hayaletin hareketi
    let angle = Math.random() * Math.PI * 2;
    const speed = 2;
    
    const moveGhost = () => {
        const x = parseFloat(newGhost.style.left);
        const y = parseFloat(newGhost.style.top);
        
        // Yön değiştirme
        angle += (Math.random() - 0.5) * 0.1;
        
        const newX = x + Math.cos(angle) * speed;
        const newY = y + Math.sin(angle) * speed;
        
        // Ekran sınırlarını kontrol et
        if (newX < 0 || newX > window.innerWidth) angle = Math.PI - angle;
        if (newY < 0 || newY > window.innerHeight) angle = -angle;
        
        newGhost.style.left = newX + 'px';
        newGhost.style.top = newY + 'px';
        
        requestAnimationFrame(moveGhost);
    };
    
    moveGhost();
    ghosts.push(newGhost);
} 