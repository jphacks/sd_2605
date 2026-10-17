import { useEffect, useState } from 'react'
import './App.css'

type HealthResponse = {
  status: string
}

function App() {
  const [health, setHealth] = useState<HealthResponse | null>(null)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    let cancelled = false

    fetch('/api/health')
      .then(async (response) => {
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`)
        }
        return response.json() as Promise<HealthResponse>
      })
      .then((data) => {
        if (!cancelled) setHealth(data)
      })
      .catch((err: unknown) => {
        if (!cancelled) {
          setError(err instanceof Error ? err.message : 'APIに接続できません')
        }
      })

    return () => {
      cancelled = true
    }
  }, [])

  return (
    <main>
      <h1>sd_2605-mobile</h1>
      <p>Rails API の状態</p>
      {health && <p className="status ok">status: {health.status}</p>}
      {error && <p className="status error">{error}</p>}
      {!health && !error && <p className="status">確認中...</p>}
    </main>
  )
}

export default App
